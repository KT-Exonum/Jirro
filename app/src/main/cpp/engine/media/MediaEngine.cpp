#include "MediaEngine.h"

#include <android/hardware_buffer.h>
#include <android/log.h>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

#define LOG_TAG "MediaEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vfx {

namespace {
constexpr int64_t kDequeueTimeoutUs = 0; // never block the media thread's pump loop
// Frames more than this far from the current codec position are treated as
// a scrub/seek rather than forward playback drift.
constexpr int64_t kSeekThresholdUs = 200'000;
constexpr int kMaxImageReaderBuffers = 4; // headroom for in-flight GPU reads (Section 6/9)
} // namespace

// ---------------------------------------------------------------------------
// VideoDecoder
// ---------------------------------------------------------------------------

VideoDecoder::~VideoDecoder() { Close(); }

bool VideoDecoder::Open(GraphicsDevice& device) {
    extractor_ = AMediaExtractor_new();
    if (!extractor_) {
        LOGE("AMediaExtractor_new failed");
        return false;
    }

    if (AMediaExtractor_setDataSource(extractor_, filePath_.c_str()) != AMEDIA_OK) {
        LOGE("AMediaExtractor_setDataSource failed for %s", filePath_.c_str());
        Close();
        return false;
    }

    const size_t trackCount = AMediaExtractor_getTrackCount(extractor_);
    AMediaFormat* videoFormat = nullptr;
    const char* mime = nullptr;
    for (size_t i = 0; i < trackCount; ++i) {
        AMediaFormat* fmt = AMediaExtractor_getTrackFormat(extractor_, i);
        const char* trackMime = nullptr;
        if (AMediaFormat_getString(fmt, AMEDIAFORMAT_KEY_MIME, &trackMime) && trackMime &&
            std::string_view(trackMime).substr(0, 6) == "video/") {
            videoTrackIndex_ = static_cast<int>(i);
            videoFormat = fmt;
            mime = trackMime;
            break;
        }
        AMediaFormat_delete(fmt);
    }

    if (videoTrackIndex_ < 0 || !videoFormat) {
        LOGE("No video track found in %s", filePath_.c_str());
        Close();
        return false;
    }

    AMediaFormat_getInt32(videoFormat, AMEDIAFORMAT_KEY_WIDTH, reinterpret_cast<int32_t*>(&width_));
    AMediaFormat_getInt32(videoFormat, AMEDIAFORMAT_KEY_HEIGHT, reinterpret_cast<int32_t*>(&height_));

    AMediaExtractor_selectTrack(extractor_, videoTrackIndex_);

    // AImageReader is the CPU-side handle for a buffer queue whose consumer
    // surface (AImageReader_getWindow) we hand straight to AMediaCodec.
    // USAGE_GPU_SAMPLED_IMAGE is what makes the resulting AHardwareBuffer
    // importable into Vulkan via vkGetAndroidHardwareBufferPropertiesANDROID
    // (Section 4: no CPU-side YUV buffer is ever produced by this path).
    media_status_t readerStatus = AImageReader_newWithUsage(
        static_cast<int32_t>(width_), static_cast<int32_t>(height_),
        AIMAGE_FORMAT_PRIVATE, AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE,
        kMaxImageReaderBuffers, &imageReader_);
    if (readerStatus != AMEDIA_OK || !imageReader_) {
        LOGE("AImageReader_newWithUsage failed: %d", readerStatus);
        AMediaFormat_delete(videoFormat);
        Close();
        return false;
    }

    ANativeWindow* outputWindow = nullptr;
    AImageReader_getWindow(imageReader_, &outputWindow);
    if (!outputWindow) {
        LOGE("AImageReader_getWindow returned null");
        AMediaFormat_delete(videoFormat);
        Close();
        return false;
    }

    codec_ = AMediaCodec_createDecoderByType(mime);
    if (!codec_) {
        LOGE("AMediaCodec_createDecoderByType failed for mime %s", mime);
        AMediaFormat_delete(videoFormat);
        Close();
        return false;
    }

    // Passing `outputWindow` here (rather than nullptr + a ByteBuffer output
    // path) is what keeps decoded frames entirely on the GPU/hardware-buffer
    // side — see AMediaCodec_configure docs: with a non-null surface the
    // codec produces COLOR_FormatSurface buffers regardless of the format
    // the bitstream itself declares.
    if (AMediaCodec_configure(codec_, videoFormat, outputWindow, nullptr, 0) != AMEDIA_OK) {
        LOGE("AMediaCodec_configure failed");
        AMediaFormat_delete(videoFormat);
        Close();
        return false;
    }
    AMediaFormat_delete(videoFormat);

    if (AMediaCodec_start(codec_) != AMEDIA_OK) {
        LOGE("AMediaCodec_start failed");
        Close();
        return false;
    }

    (void)device; // reserved: some devices need capability queries here before configure
    isOpen_ = true;
    inputEOS_ = false;
    lastDecodedTimeUs_ = -1;
    LOGI("VideoDecoder opened %s (%ux%u)", filePath_.c_str(), width_, height_);
    return true;
}

void VideoDecoder::Close() {
    if (codec_) {
        AMediaCodec_stop(codec_);
        AMediaCodec_delete(codec_);
        codec_ = nullptr;
    }
    if (imageReader_) {
        AImageReader_delete(imageReader_);
        imageReader_ = nullptr;
    }
    if (extractor_) {
        AMediaExtractor_delete(extractor_);
        extractor_ = nullptr;
    }
    isOpen_ = false;
}

bool VideoDecoder::SeekTo(double sourceTimeSeconds) {
    if (!isOpen_) return false;
    pendingSeekTimeUs_ = static_cast<int64_t>(std::max(0.0, sourceTimeSeconds) * 1'000'000.0);
    return true;
}

bool VideoDecoder::PumpInput() {
    if (pendingSeekTimeUs_ >= 0) {
        // Section 7 frame-accurate seeking: land on the nearest sync sample
        // at or before the target, then flush so the codec discards any
        // frames it had already buffered from before the seek — otherwise
        // DequeueFrame would surface pre-seek frames first.
        AMediaExtractor_seekTo(extractor_, pendingSeekTimeUs_, AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC);
        AMediaCodec_flush(codec_);
        inputEOS_ = false;
        lastDecodedTimeUs_ = -1;
        pendingSeekTimeUs_ = -1;
    }

    if (inputEOS_) return false;

    const ssize_t inIndex = AMediaCodec_dequeueInputBuffer(codec_, kDequeueTimeoutUs);
    if (inIndex < 0) return false; // no free input buffer right now, try again next pump

    size_t bufSize = 0;
    uint8_t* buf = AMediaCodec_getInputBuffer(codec_, inIndex, &bufSize);
    if (!buf) return false;

    const ssize_t sampleSize = AMediaExtractor_readSampleData(extractor_, buf, bufSize);
    if (sampleSize < 0) {
        AMediaCodec_queueInputBuffer(codec_, inIndex, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
        inputEOS_ = true;
        return true;
    }

    const int64_t sampleTimeUs = AMediaExtractor_getSampleTime(extractor_);
    AMediaCodec_queueInputBuffer(codec_, inIndex, 0, static_cast<size_t>(sampleSize), sampleTimeUs, 0);
    AMediaExtractor_advance(extractor_);
    return true;
}

bool VideoDecoder::DrainOutput(int64_t* outPtsUs) {
    AMediaCodecBufferInfo info{};
    const ssize_t outIndex = AMediaCodec_dequeueOutputBuffer(codec_, &info, kDequeueTimeoutUs);
    if (outIndex < 0) return false; // AMEDIACODEC_INFO_TRY_AGAIN_LATER or a format/buffers-changed
                                     // event — Phase 6 could act on OUTPUT_FORMAT_CHANGED for
                                     // resolution changes mid-stream; not needed for Phase 2.

    // `render = true` is what pushes the decoded buffer into the
    // AImageReader's queue as an AHardwareBuffer-backed AImage; releasing
    // with render=false would just drop the frame.
    const bool isEOS = (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0;
    AMediaCodec_releaseOutputBuffer(codec_, outIndex, info.size > 0 && !isEOS);
    if (outPtsUs) *outPtsUs = info.presentationTimeUs;
    return info.size > 0 && !isEOS;
}

std::optional<DecodedFrame> VideoDecoder::DequeueFrame(GraphicsDevice& device) {
    if (!isOpen_) return std::nullopt;

    // Keep the input queue fed before draining, otherwise a full output
    // queue with a starved input queue can stall the codec.
    PumpInput();

    int64_t ptsUs = -1;
    if (!DrainOutput(&ptsUs)) return std::nullopt;

    AImage* rawImage = nullptr;
    const media_status_t acquireStatus = AImageReader_acquireLatestImage(imageReader_, &rawImage);
    if (acquireStatus != AMEDIA_OK || !rawImage) {
        // Buffer was rendered to the surface but the reader hasn't surfaced
        // it yet (rare timing race) — caller retries on the next pump.
        return std::nullopt;
    }

    // Wrap in shared_ptr with AImage_delete as the deleter so FrameCache can
    // hold this past the current pump call — deleting it releases the
    // buffer back to the AImageReader queue for reuse (Section 5/6 pooling,
    // one level down at the platform-buffer layer instead of GraphicsDevice's).
    std::shared_ptr<AImage> image(rawImage, [](AImage* img) { AImage_delete(img); });

    AHardwareBuffer* hardwareBuffer = nullptr;
    if (AImage_getHardwareBuffer(image.get(), &hardwareBuffer) != AMEDIA_OK || !hardwareBuffer) {
        LOGE("AImage_getHardwareBuffer failed");
        return std::nullopt;
    }

    GraphicsDevice::HardwareBufferHandle hwHandle{hardwareBuffer};
    Result<TextureHandle> imported = device.ImportHardwareBuffer(hwHandle, width_, height_);
    if (!imported) {
        LOGE("ImportHardwareBuffer failed: %s", imported.error.c_str());
        return std::nullopt;
    }

    lastDecodedTimeUs_ = ptsUs;

    DecodedFrame frame;
    frame.texture = imported.value;
    frame.presentationTimeUs = ptsUs;
    frame.width = width_;
    frame.height = height_;
    frame.ownedImage = std::move(image);
    return frame;
}

// ---------------------------------------------------------------------------
// DecoderPool
// ---------------------------------------------------------------------------

VideoDecoder* DecoderPool::Acquire(const std::string& clipSourcePath, GraphicsDevice& device) {
    ++tickCounter_;
    if (auto it = active_.find(clipSourcePath); it != active_.end()) {
        it->second.lastUsedTickCounter = tickCounter_;
        return it->second.decoder.get();
    }

    if (active_.size() >= config_.maxConcurrentDecoders) {
        // Evict the least-recently-used decoder (Section 8: bound by device
        // capability, not clip count) to make room.
        auto lruIt = std::min_element(active_.begin(), active_.end(), [](const auto& a, const auto& b) {
            return a.second.lastUsedTickCounter < b.second.lastUsedTickCounter;
        });
        if (lruIt != active_.end()) {
            LOGI("DecoderPool: evicting %s to make room for %s", lruIt->first.c_str(),
                 clipSourcePath.c_str());
            active_.erase(lruIt);
        }
    }

    auto decoder = std::make_unique<VideoDecoder>(clipSourcePath);
    if (!decoder->Open(device)) {
        LOGE("DecoderPool: failed to open %s", clipSourcePath.c_str());
        return nullptr;
    }
    VideoDecoder* raw = decoder.get();
    active_[clipSourcePath] = Entry{std::move(decoder), tickCounter_};
    return raw;
}

void DecoderPool::ReleaseIdleExcept(const std::vector<std::string>& keepPaths) {
    for (auto it = active_.begin(); it != active_.end();) {
        const bool keep = std::find(keepPaths.begin(), keepPaths.end(), it->first) != keepPaths.end();
        it = keep ? std::next(it) : active_.erase(it);
    }
}

void DecoderPool::CloseAll() { active_.clear(); }

// ---------------------------------------------------------------------------
// FrameCache
// ---------------------------------------------------------------------------

std::optional<DecodedFrame> FrameCache::Get(const std::string& clipId, double sourceTimeSeconds) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(clipId);
    if (it == cache_.end() || it->second.empty()) return std::nullopt;

    const Entry* best = nullptr;
    double bestDelta = std::numeric_limits<double>::max();
    for (const auto& entry : it->second) {
        const double delta = std::abs(entry.sourceTimeSeconds - sourceTimeSeconds);
        if (delta < bestDelta) {
            bestDelta = delta;
            best = &entry;
        }
    }
    // Half a frame at a generous 60fps ceiling; a real implementation should
    // derive this from the clip's actual frame rate instead of a constant.
    constexpr double kMaxAcceptableDeltaSeconds = 1.0 / 120.0;
    if (best && bestDelta <= kMaxAcceptableDeltaSeconds) return best->frame;
    return std::nullopt;
}

void FrameCache::Put(const std::string& clipId, DecodedFrame frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    const double sourceTimeSeconds = frame.presentationTimeUs / 1'000'000.0;
    currentBytes_ += EstimateBytes(frame);
    cache_[clipId].push_back(Entry{std::move(frame), sourceTimeSeconds});
}

void FrameCache::EvictOutsideWindow() {
    std::lock_guard<std::mutex> lock(mutex_);
    // Section 9: keep only [playhead - framesBehind*dt, playhead +
    // framesAhead*dt] worth of frames per clip. Without a reliable per-clip
    // frame rate handy here, approximate the window in wall-clock seconds
    // (generous enough at typical 24-60fps that it still bounds memory
    // sensibly; Phase 6 should key this off the clip's actual frame rate).
    constexpr double kApproxFrameSeconds = 1.0 / 24.0;
    const double behind = config_.framesBehind * kApproxFrameSeconds;
    const double ahead = config_.framesAhead * kApproxFrameSeconds;

    for (auto& [clipId, entries] : cache_) {
        std::erase_if(entries, [&](const Entry& e) {
            const bool outside =
                e.sourceTimeSeconds < playheadHint_ - behind || e.sourceTimeSeconds > playheadHint_ + ahead;
            if (outside) currentBytes_ -= EstimateBytes(e.frame);
            return outside;
        });
    }

    // Hard memory ceiling (Section 9 "configurable memory limits"): if still
    // over budget after windowing (e.g. very high resolution footage), drop
    // the frames furthest from the playhead across all clips.
    while (currentBytes_ > config_.maxBytes) {
        std::string worstClip;
        size_t worstIdx = 0;
        double worstDelta = -1.0;
        for (auto& [clipId, entries] : cache_) {
            for (size_t i = 0; i < entries.size(); ++i) {
                const double delta = std::abs(entries[i].sourceTimeSeconds - playheadHint_);
                if (delta > worstDelta) {
                    worstDelta = delta;
                    worstClip = clipId;
                    worstIdx = i;
                }
            }
        }
        if (worstDelta < 0.0) break; // nothing left to evict
        auto& entries = cache_[worstClip];
        currentBytes_ -= EstimateBytes(entries[worstIdx].frame);
        entries.erase(entries.begin() + static_cast<long>(worstIdx));
    }
}

// ---------------------------------------------------------------------------
// ProxyCache
// ---------------------------------------------------------------------------

std::optional<ProxyFrame> ProxyCache::Get(const std::string& clipId, double sourceTimeSeconds) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(clipId);
    if (it == cache_.end() || it->second.empty()) return std::nullopt;

    const Entry* best = nullptr;
    double bestDelta = std::numeric_limits<double>::max();
    for (const auto& entry : it->second) {
        const double delta = std::abs(entry.sourceTimeSeconds - sourceTimeSeconds);
        if (delta < bestDelta) {
            bestDelta = delta;
            best = &entry;
        }
    }
    constexpr double kMaxAcceptableDeltaSeconds = 1.0 / 120.0;
    if (best && bestDelta <= kMaxAcceptableDeltaSeconds) return best->frame;
    return std::nullopt;
}

void ProxyCache::Put(const std::string& clipId, ProxyFrame frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    const double sourceTimeSeconds = frame.presentationTimeUs / 1'000'000.0;
    // Rough byte estimate for proxy (lower res)
    const size_t frameBytes = static_cast<size_t>(frame.width) * frame.height * 2;
    currentBytes_ += frameBytes;
    cache_[clipId].push_back(Entry{std::move(frame), sourceTimeSeconds});
}

void ProxyCache::EvictOutsideWindow() {
    std::lock_guard<std::mutex> lock(mutex_);
    constexpr double kApproxFrameSeconds = 1.0 / 24.0;
    const double behind = config_.framesBehind * kApproxFrameSeconds;
    const double ahead = config_.framesAhead * kApproxFrameSeconds;

    for (auto& [clipId, entries] : cache_) {
        std::erase_if(entries, [&](const Entry& e) {
            const bool outside =
                e.sourceTimeSeconds < playheadHint_ - behind || e.sourceTimeSeconds > playheadHint_ + ahead;
            if (outside) {
                const size_t frameBytes = static_cast<size_t>(e.frame.width) * e.frame.height * 2;
                currentBytes_ -= frameBytes;
            }
            return outside;
        });
    }

    while (currentBytes_ > config_.maxBytes) {
        std::string worstClip;
        size_t worstIdx = 0;
        double worstDelta = -1.0;
        for (auto& [clipId, entries] : cache_) {
            for (size_t i = 0; i < entries.size(); ++i) {
                const double delta = std::abs(entries[i].sourceTimeSeconds - playheadHint_);
                if (delta > worstDelta) {
                    worstDelta = delta;
                    worstClip = clipId;
                    worstIdx = i;
                }
            }
        }
        if (worstDelta < 0.0) break;
        auto& entries = cache_[worstClip];
        const size_t frameBytes = static_cast<size_t>(entries[worstIdx].frame.width) * entries[worstIdx].frame.height * 2;
        currentBytes_ -= frameBytes;
        entries.erase(entries.begin() + static_cast<long>(worstIdx));
    }
}

// ---------------------------------------------------------------------------
// AudioCache
// ---------------------------------------------------------------------------

void AudioCache::LoadAudio(const std::string& clipId, const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (audioCache_.find(clipId) != audioCache_.end()) return; // already loaded

    // In a real implementation, this would use AMediaExtractor/AMediaCodec to decode
    // audio to PCM. For now, we create a placeholder.
    AudioClipData clip;
    clip.clipId = clipId;
    clip.sampleRate = 48000;
    clip.channels = 2;
    clip.duration = 0.0; // Would be read from file
    clip.isLoaded = true;
    // Placeholder: allocate silent audio for duration estimation
    clip.pcmData.assign(static_cast<size_t>(clip.duration * clip.sampleRate * clip.channels), 0.0f);
    
    const size_t bytes = clip.pcmData.size() * sizeof(float);
    currentBytes_ += bytes;
    audioCache_[clipId] = std::move(clip);
}

void AudioCache::UnloadAudio(const std::string& clipId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = audioCache_.find(clipId);
    if (it != audioCache_.end()) {
        currentBytes_ -= it->second.pcmData.size() * sizeof(float);
        audioCache_.erase(it);
    }
}

// ---------------------------------------------------------------------------
// MediaEngine: Audio/Proxy handling
// ---------------------------------------------------------------------------

void MediaEngine::SetActiveAudioClips(std::vector<ActiveAudioRequest> clips, double playheadTimelineSeconds) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    pendingAudioRequests_ = std::move(clips);
}

void MediaEngine::SetActiveProxies(std::vector<ActiveProxyRequest> clips) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    pendingProxyRequests_ = std::move(clips);
}

void MediaEngine::DecodeAudioFile(const std::string& clipId, const std::string& filePath) {
    // Real implementation would:
    // 1. AMediaExtractor for audio track
    // 2. AMediaCodec decoder (or use AMediaExtractor's direct read for simple formats)
    // 3. Resample to 48kHz stereo float
    // 4. Store in AudioCache
    audioCache_.LoadAudio(clipId, filePath);
}

void MediaEngine::GenerateProxy(const ActiveProxyRequest& request) {
    // Real implementation would:
    // 1. AMediaExtractor + AMediaCodec decoder for source
    // 2. Scale frames to proxy resolution (MediaCodec can output to surface of target size)
    // 3. AMediaCodec encoder for proxy (video/avc or video/hevc)
    // 4. Write to proxy file path
    // For now, just mark as needing generation
    LOGI("Proxy generation requested for %s -> %s", request.sourceFilePath.c_str(), request.proxyFilePath.c_str());
}

void MediaEngine::Start() {
    if (running_.exchange(true)) return;
    mediaThread_ = std::thread(&MediaEngine::MediaThreadMain, this);
}

void MediaEngine::Stop() {
    if (!running_.exchange(false)) return;
    if (mediaThread_.joinable()) mediaThread_.join();
    decoderPool_.CloseAll();
}

void MediaEngine::SetActiveClips(std::vector<ActiveClipRequest> clips, double playheadTimelineSeconds) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    pendingRequests_ = std::move(clips);
    frameCache_.SetPlayheadHint(playheadTimelineSeconds);
}

void MediaEngine::MediaThreadMain() {
    while (running_.load(std::memory_order_acquire)) {
        std::vector<ActiveClipRequest> clipRequests;
        std::vector<ActiveAudioRequest> audioRequests;
        std::vector<ActiveProxyRequest> proxyRequests;
        {
            std::lock_guard<std::mutex> lock(requestMutex_);
            clipRequests = pendingRequests_;
            audioRequests = pendingAudioRequests_;
            proxyRequests = pendingProxyRequests_;
        }

        // Process proxy generation requests
        for (const auto& req : proxyRequests) {
            GenerateProxy(req);
        }

        // Process audio decoding requests
        for (const auto& req : audioRequests) {
            audioCache_.LoadAudio(req.clipId, req.sourceFilePath);
        }

        // Process video decode requests
        std::vector<std::string> keepPaths;
        keepPaths.reserve(clipRequests.size());
        for (const auto& req : clipRequests) keepPaths.push_back(req.sourceFilePath);
        decoderPool_.ReleaseIdleExcept(keepPaths);

        for (const auto& req : clipRequests) {
            VideoDecoder* decoder = decoderPool_.Acquire(req.sourceFilePath, device_);
            if (!decoder) continue;

            // Only seek when we've drifted meaningfully from the codec's
            // current position — playback advances frame-by-frame without
            // re-seeking every tick, but scrubbing jumps do trigger one.
            const int64_t requestedUs = static_cast<int64_t>(req.sourceTimeSeconds * 1'000'000.0);
            const int64_t lastUs = static_cast<int64_t>(decoder->LastDecodedTimeSeconds() * 1'000'000.0);
            if (decoder->LastDecodedTimeSeconds() < 0 ||
                std::abs(requestedUs - lastUs) > kSeekThresholdUs) {
                decoder->SeekTo(req.sourceTimeSeconds);
            }

            if (auto frame = decoder->DequeueFrame(device_)) {
                frameCache_.Put(req.clipId, std::move(*frame));
            }
        }

        // Evict proxy cache
        proxyCache_.SetPlayheadHint(clipRequests.empty() ? 0.0 : clipRequests[0].sourceTimeSeconds);
        proxyCache_.EvictOutsideWindow();

        frameCache_.EvictOutsideWindow();

        // Not vsync-paced like the engine thread — this just needs to keep
        // up with playback framerate without busy-spinning. A production
        // build should wake this via a condvar when SetActiveClips changes
        // rather than polling.
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}

} // namespace vfx
