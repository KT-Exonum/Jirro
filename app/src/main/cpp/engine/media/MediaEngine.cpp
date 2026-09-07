#include "MediaEngine.h"

#include <android/log.h>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>

#include <algorithm>
#include <chrono>

#define LOG_TAG "MediaEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace vfx {

VideoDecoder::VideoDecoder(std::string filePath) : filePath_(std::move(filePath)) {}

VideoDecoder::~VideoDecoder() {
    Close();
}

bool VideoDecoder::Open(GraphicsDevice& device) {
    (void)device; // Used by ImportHardwareBuffer
    
    extractor_ = AMediaExtractor_new();
    if (!extractor_) {
        LOGE("Failed to create media extractor for %s", filePath_.c_str());
        return false;
    }

    media_status_t status = AMediaExtractor_setDataSource(extractor_, filePath_.c_str());
    if (status != AMEDIA_OK) {
        LOGE("Failed to set data source for %s: %d", filePath_.c_str(), status);
        AMediaExtractor_delete(extractor_);
        extractor_ = nullptr;
        return false;
    }

    int trackCount = AMediaExtractor_getTrackCount(extractor_);
    for (int i = 0; i < trackCount; ++i) {
        AMediaFormat* fmt = AMediaExtractor_getTrackFormat(extractor_, i);
        const char* mime = nullptr;
        AMediaFormat_getString(fmt, AMEDIAFORMAT_KEY_MIME, &mime);
        if (mime && strncmp(mime, "video/", 6) == 0) {
            videoTrackIndex_ = i;
            AMediaExtractor_selectTrack(extractor_, i);

            int32_t width = 0, height = 0;
            AMediaFormat_getInteger(fmt, AMEDIAFORMAT_KEY_WIDTH, &width);
            AMediaFormat_getInteger(fmt, AMEDIAFORMAT_KEY_HEIGHT, &height);
            width_ = static_cast<uint32_t>(width);
            height_ = static_cast<uint32_t>(height);

            int64_t durationUs = 0;
            AMediaFormat_getLongLong(fmt, AMEDIAFORMAT_KEY_DURATION, &durationUs);

            codec_ = AMediaCodec_createDecoderByType(mime);
            if (!codec_) {
                LOGE("Failed to create decoder for %s", mime);
                AMediaFormat_delete(fmt);
                Close();
                return false;
            }

            // Create ImageReader with GPU-compatible format
            imageReader_ = AImageReader_new(width_, height_, AIMAGE_FORMAT_YUV_420_888, 4);
            if (!imageReader_) {
                LOGE("Failed to create AImageReader");
                AMediaFormat_delete(fmt);
                Close();
                return false;
            }

            ANativeWindow* surface = AImageReader_getWindow(imageReader_);
            if (!surface) {
                LOGE("Failed to get ANativeWindow from AImageReader");
                AMediaFormat_delete(fmt);
                Close();
                return false;
            }

            media_status_t cs = AMediaCodec_configure(codec_, fmt, surface, nullptr, 0);
            ANativeWindow_release(surface);
            AMediaFormat_delete(fmt);
            if (cs != AMEDIA_OK) {
                LOGE("Failed to configure codec: %d", cs);
                Close();
                return false;
            }

            if (AMediaCodec_start(codec_) != AMEDIA_OK) {
                LOGE("Failed to start codec");
                Close();
                return false;
            }

            isOpen_ = true;
            inputEOS_ = false;
            lastDecodedTimeUs_ = -1;
            pendingSeekTimeUs_ = -1;
            LOGI("VideoDecoder opened: %s (%ux%u)", filePath_.c_str(), width_, height_);
            return true;
        }
        AMediaFormat_delete(fmt);
    }

    LOGE("No video track found in %s", filePath_.c_str());
    Close();
    return false;
}

void VideoDecoder::Close() {
    if (codec_) {
        AMediaCodec_stop(codec_);
        AMediaCodec_delete(codec_);
        codec_ = nullptr;
    }
    if (extractor_) {
        AMediaExtractor_delete(extractor_);
        extractor_ = nullptr;
    }
    if (imageReader_) {
        AImageReader_delete(imageReader_);
        imageReader_ = nullptr;
    }
    isOpen_ = false;
    inputEOS_ = false;
    lastDecodedTimeUs_ = -1;
    pendingSeekTimeUs_ = -1;
}

bool VideoDecoder::SeekTo(double sourceTimeSeconds) {
    if (!isOpen_ || !extractor_) return false;

    int64_t seekTimeUs = static_cast<int64_t>(sourceTimeSeconds * 1'000'000.0);
    pendingSeekTimeUs_ = seekTimeUs;

    // Seek extractor to nearest sync sample
    media_status_t status = AMediaExtractor_seekTo(extractor_, seekTimeUs, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
    if (status != AMEDIA_OK) {
        LOGE("Extractor seek failed: %d", status);
        return false;
    }

    // Flush codec to clear stale frames
    if (codec_) {
        AMediaCodec_flush(codec_);
    }

    inputEOS_ = false;
    lastDecodedTimeUs_ = -1;
    return true;
}

bool VideoDecoder::PumpInput() {
    if (!codec_ || inputEOS_) return false;

    ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(codec_, 0);
    if (inputIndex < 0) return false;

    size_t bufSize = 0;
    uint8_t* buf = AMediaCodec_getInputBuffer(codec_, inputIndex, &bufSize);
    if (!buf) return false;

    ssize_t sampleSize = AMediaExtractor_readSampleData(extractor_, buf, static_cast<int32_t>(bufSize));
    if (sampleSize < 0) {
        AMediaCodec_queueInputBuffer(codec_, inputIndex, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
        inputEOS_ = true;
        return true;
    }

    int64_t pts = AMediaExtractor_getSampleTime(extractor_);
    
    // Handle pending seek
    if (pendingSeekTimeUs_ >= 0) {
        // Skip frames before seek point
        if (pts < pendingSeekTimeUs_) {
            AMediaExtractor_advance(extractor_);
            AMediaCodec_queueInputBuffer(codec_, inputIndex, 0, 0, 0, 0);
            return true;
        }
        pendingSeekTimeUs_ = -1;
    }

    AMediaCodec_queueInputBuffer(codec_, inputIndex, 0, static_cast<size_t>(sampleSize), pts, 0);
    AMediaExtractor_advance(extractor_);
    return true;
}

bool VideoDecoder::DrainOutput(int64_t* outPtsUs) {
    if (!codec_) return false;

    AMediaCodecBufferInfo info;
    ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(codec_, &info, 0);
    if (outputIndex < 0) return false;

    if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
        AMediaCodec_releaseOutputBuffer(codec_, outputIndex, false);
        return false;
    }

    if (info.size > 0) {
        *outPtsUs = info.presentationTimeUs;
        AMediaCodec_releaseOutputBuffer(codec_, outputIndex, true); // render to ImageReader
        return true;
    }

    AMediaCodec_releaseOutputBuffer(codec_, outputIndex, false);
    return false;
}

std::optional<DecodedFrame> VideoDecoder::DequeueFrame(GraphicsDevice& device) {
    if (!isOpen_) return std::nullopt;

    // Pump input until we have a frame ready
    int64_t ptsUs = 0;
    while (PumpInput() || DrainOutput(&ptsUs)) {
        if (ptsUs > 0) {
            // Acquire image from reader
            AImage* image = nullptr;
            media_status_t status = AImageReader_acquireLatestImage(imageReader_, &image);
            if (status == AMEDIA_OK && image) {
                // Import hardware buffer into Vulkan
                AHardwareBuffer* hardwareBuffer = nullptr;
                status = AImage_getHardwareBuffer(image, &hardwareBuffer);
                if (status == AMEDIA_OK && hardwareBuffer) {
                    TextureDesc desc;
                    desc.width = width_;
                    desc.height = height_;
                    desc.format = PixelFormat::YCbCr420P; // YUV format
                    desc.usage = TextureUsage::Sampled;
                    desc.debugName = "video_frame_" + std::to_string(ptsUs);

                    auto result = device.ImportHardwareBuffer(hardwareBuffer, desc);
                    if (result) {
                        DecodedFrame frame;
                        frame.texture = result.value;
                        frame.presentationTimeUs = ptsUs;
                        frame.width = width_;
                        frame.height = height_;
                        frame.ownedImage = std::shared_ptr<AImage>(image, [](AImage* img) {
                            AImage_delete(img);
                        });
                        lastDecodedTimeUs_ = ptsUs;
                        return frame;
                    }
                }
                AImage_delete(image);
            }
        }
    }

    return std::nullopt;
}

// ============================================================================
// DecoderPool
// ============================================================================

VideoDecoder* DecoderPool::Acquire(const std::string& clipSourcePath, GraphicsDevice& device) {
    auto it = active_.find(clipSourcePath);
    if (it != active_.end()) {
        it->second.lastUsedTickCounter = tickCounter_++;
        return it->second.decoder.get();
    }

    // Need to open a new decoder
    if (active_.size() >= config_.maxConcurrentDecoders) {
        // Evict LRU
        auto lruIt = std::min_element(active_.begin(), active_.end(),
            [](const auto& a, const auto& b) {
                return a.second.lastUsedTickCounter < b.second.lastUsedTickCounter;
            });
        if (lruIt != active_.end()) {
            active_.erase(lruIt);
        }
    }

    auto decoder = std::make_unique<VideoDecoder>(clipSourcePath);
    if (!decoder->Open(device)) {
        LOGE("Failed to open decoder for %s", clipSourcePath.c_str());
        return nullptr;
    }

    Entry entry;
    entry.decoder = std::move(decoder);
    entry.lastUsedTickCounter = tickCounter_++;
    auto [newIt, inserted] = active_.emplace(clipSourcePath, std::move(entry));
    return newIt->second.decoder.get();
}

void DecoderPool::ReleaseIdleExcept(const std::vector<std::string>& keepPaths) {
    std::unordered_set<std::string> keepSet(keepPaths.begin(), keepPaths.end());
    
    for (auto it = active_.begin(); it != active_.end();) {
        if (keepSet.find(it->first) == keepSet.end()) {
            it = active_.erase(it);
        } else {
            ++it;
        }
    }
}

void DecoderPool::CloseAll() {
    active_.clear();
}

// ============================================================================
// FrameCache
// ============================================================================

std::optional<DecodedFrame> FrameCache::Get(const std::string& clipId, double sourceTimeSeconds) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(clipId);
    if (it == cache_.end()) return std::nullopt;

    const auto& entries = it->second;
    if (entries.empty()) return std::nullopt;

    // Find nearest frame by time
    const Entry* best = nullptr;
    double bestDiff = 1e9;
    for (const auto& entry : entries) {
        double diff = std::abs(entry.sourceTimeSeconds - sourceTimeSeconds);
        if (diff < bestDiff) {
            bestDiff = diff;
            best = &entry;
        }
    }

    if (best && bestDiff < 0.5) { // Within 0.5 seconds
        return best->frame;
    }
    return std::nullopt;
}

void FrameCache::Put(const std::string& clipId, DecodedFrame frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Estimate frame size for memory accounting
    size_t frameBytes = EstimateBytes(frame);
    
    // Check memory limit
    while (currentBytes_ + frameBytes > config_.maxBytes && !cache_.empty()) {
        // Evict oldest clip's oldest frame
        for (auto& [cid, entries] : cache_) {
            if (!entries.empty()) {
                currentBytes_ -= EstimateBytes(entries.front().frame);
                entries.erase(entries.begin());
                if (entries.empty()) {
                    cache_.erase(cid);
                }
                break;
            }
        }
    }

    Entry entry;
    entry.frame = std::move(frame);
    entry.sourceTimeSeconds = 0.0; // Will be set by caller
    
    cache_[clipId].push_back(std::move(entry));
    currentBytes_ += frameBytes;
}

void FrameCache::EvictOutsideWindow() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    double windowStart = playheadHint_ - config_.framesBehind / frameRate_;
    double windowEnd = playheadHint_ + config_.framesAhead / frameRate_;
    
    for (auto& [clipId, entries] : cache_) {
        entries.erase(std::remove_if(entries.begin(), entries.end(),
            [windowStart, windowEnd](const Entry& e) {
                return e.sourceTimeSeconds < windowStart || e.sourceTimeSeconds > windowEnd;
            }), entries.end());
    }
}

// ============================================================================
// ProxyCache
// ============================================================================

std::optional<ProxyFrame> ProxyCache::Get(const std::string& clipId, double sourceTimeSeconds) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(clipId);
    if (it == cache_.end()) return std::nullopt;

    const auto& entries = it->second;
    if (entries.empty()) return std::nullopt;

    const Entry* best = nullptr;
    double bestDiff = 1e9;
    for (const auto& entry : entries) {
        double diff = std::abs(entry.sourceTimeSeconds - sourceTimeSeconds);
        if (diff < bestDiff) {
            bestDiff = diff;
            best = &entry;
        }
    }

    if (best && bestDiff < 0.5) {
        return best->frame;
    }
    return std::nullopt;
}

void ProxyCache::Put(const std::string& clipId, ProxyFrame frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t frameBytes = frame.width * frame.height * 2; // YUV 4:2:0
    
    while (currentBytes_ + frameBytes > config_.maxBytes && !cache_.empty()) {
        for (auto& [cid, entries] : cache_) {
            if (!entries.empty()) {
                currentBytes_ -= entries.front().frame.width * entries.front().frame.height * 2;
                entries.erase(entries.begin());
                if (entries.empty()) {
                    cache_.erase(cid);
                }
                break;
            }
        }
    }

    Entry entry;
    entry.frame = std::move(frame);
    entry.sourceTimeSeconds = 0.0;
    
    cache_[clipId].push_back(std::move(entry));
    currentBytes_ += frameBytes;
}

void ProxyCache::EvictOutsideWindow() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [clipId, entries] : cache_) {
        entries.erase(std::remove_if(entries.begin(), entries.end(),
            [this](const Entry& e) {
                return e.sourceTimeSeconds < playheadHint_ - config_.framesBehind / 30.0 ||
                       e.sourceTimeSeconds > playheadHint_ + config_.framesAhead / 30.0;
            }), entries.end());
    }
}

// ============================================================================
// AudioCache
// ============================================================================

void AudioCache::LoadAudio(const std::string& clipId, const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if already loaded
    if (auto it = audioCache_.find(clipId); it != audioCache_.end() && it->second.isLoaded) {
        return;
    }
    
    AudioClipData clipData = DecodeAudioWithMediaCodec(filePath);
    clipData.clipId = clipId;
    
    size_t bytes = clipData.pcmData.size() * sizeof(float);
    currentBytes_ += bytes;
    audioCache_[clipId] = std::move(clipData);
}

void AudioCache::UnloadAudio(const std::string& clipId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = audioCache_.find(clipId); it != audioCache_.end()) {
        currentBytes_ -= it->second.pcmData.size() * sizeof(float);
        audioCache_.erase(it);
    }
}

AudioClipData AudioCache::DecodeAudioWithMediaCodec(const std::string& filePath) {
    AudioClipData clipData;
    
    AMediaExtractor* extractor = AMediaExtractor_new();
    if (!extractor) return clipData;
    
    if (AMediaExtractor_setDataSource(extractor, filePath.c_str()) != AMEDIA_OK) {
        AMediaExtractor_delete(extractor);
        return clipData;
    }
    
    int trackCount = AMediaExtractor_getTrackCount(extractor);
    for (int i = 0; i < trackCount; ++i) {
        AMediaFormat* fmt = AMediaExtractor_getTrackFormat(extractor, i);
        const char* mime = nullptr;
        AMediaFormat_getString(fmt, AMEDIAFORMAT_KEY_MIME, &mime);
        if (mime && strncmp(mime, "audio/", 6) == 0) {
            AMediaExtractor_selectTrack(extractor, i);
            
            int32_t sampleRate = 48000;
            int32_t channels = 2;
            AMediaFormat_getInteger(fmt, AMEDIAFORMAT_KEY_SAMPLE_RATE, &sampleRate);
            AMediaFormat_getInteger(fmt, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);
            clipData.sampleRate = sampleRate;
            clipData.channels = channels;
            
            int64_t durationUs = 0;
            AMediaFormat_getLongLong(fmt, AMEDIAFORMAT_KEY_DURATION, &durationUs);
            clipData.duration = durationUs / 1'000'000.0;
            
            AMediaCodec* codec = AMediaCodec_createDecoderByType(mime);
            if (!codec) {
                AMediaFormat_delete(fmt);
                AMediaExtractor_delete(extractor);
                return clipData;
            }
            
            if (AMediaCodec_configure(codec, fmt, nullptr, nullptr, 0) != AMEDIA_OK) {
                AMediaCodec_delete(codec);
                AMediaFormat_delete(fmt);
                AMediaExtractor_delete(extractor);
                return clipData;
            }
            
            if (AMediaCodec_start(codec) != AMEDIA_OK) {
                AMediaCodec_delete(codec);
                AMediaFormat_delete(fmt);
                AMediaExtractor_delete(extractor);
                return clipData;
            }
            
            AMediaFormat_delete(fmt);
            
            // Decode all audio frames
            std::vector<float> allSamples;
            bool eos = false;
            
            while (!eos) {
                ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(codec, 10000);
                if (inputIndex >= 0) {
                    size_t bufSize = 0;
                    uint8_t* buf = AMediaCodec_getInputBuffer(codec, inputIndex, &bufSize);
                    if (buf) {
                        ssize_t sampleSize = AMediaExtractor_readSampleData(extractor, buf, static_cast<int32_t>(bufSize));
                        if (sampleSize < 0) {
                            AMediaCodec_queueInputBuffer(codec, inputIndex, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                        } else {
                            int64_t pts = AMediaExtractor_getSampleTime(extractor);
                            AMediaCodec_queueInputBuffer(codec, inputIndex, 0, static_cast<size_t>(sampleSize), pts, 0);
                            AMediaExtractor_advance(extractor);
                        }
                    }
                }
                
                AMediaCodecBufferInfo info;
                ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(codec, &info, 10000);
                if (outputIndex >= 0) {
                    if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
                        AMediaCodec_releaseOutputBuffer(codec, outputIndex, false);
                        eos = true;
                    } else if (info.size > 0) {
                        size_t bufSize = 0;
                        uint8_t* buf = AMediaCodec_getOutputBuffer(codec, outputIndex, &bufSize);
                        if (buf) {
                            size_t sampleCount = info.size / sizeof(int16_t);
                            int16_t* samples16 = reinterpret_cast<int16_t*>(buf);
                            for (size_t j = 0; j < sampleCount; ++j) {
                                allSamples.push_back(static_cast<float>(samples16[j]) / 32768.0f);
                            }
                        }
                        AMediaCodec_releaseOutputBuffer(codec, outputIndex, false);
                    }
                }
            }
            
            clipData.pcmData = std::move(allSamples);
            clipData.isLoaded = true;
            
            AMediaCodec_stop(codec);
            AMediaCodec_delete(codec);
            break;
        }
        AMediaFormat_delete(fmt);
    }
    
    AMediaExtractor_delete(extractor);
    return clipData;
}

// ============================================================================
// MediaEngine
// ============================================================================

MediaEngine::MediaEngine(GraphicsDevice& device, DecoderPoolConfig decoderConfig, FrameCacheConfig cacheConfig)
    : device_(device), decoderPool_(decoderConfig), frameCache_(cacheConfig), audioCache_({}), proxyCache_({}) {}

MediaEngine::~MediaEngine() {
    Stop();
}

void MediaEngine::Start() {
    if (running_.exchange(true)) return;
    stopSource_ = std::stop_source{};
    mediaThread_ = std::jthread(&MediaEngine::MediaThreadMain, this, stopSource_.get_token());
}

void MediaEngine::Stop() {
    if (!running_.exchange(false)) return;
    stopSource_.request_stop();
    if (mediaThread_.joinable()) mediaThread_.join();
    decoderPool_.CloseAll();
}

void MediaEngine::SetActiveClips(std::vector<ActiveClipRequest> clips, double playheadTimelineSeconds) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    pendingRequests_ = std::move(clips);
    frameCache_.SetPlayheadHint(playheadTimelineSeconds);
}

void MediaEngine::SetActiveAudioClips(std::vector<ActiveAudioRequest> clips, double playheadTimelineSeconds) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    pendingAudioRequests_ = std::move(clips);
}

void MediaEngine::SetActiveProxies(std::vector<ActiveProxyRequest> clips) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    pendingProxyRequests_ = std::move(clips);
}

void MediaEngine::MediaThreadMain() {
    LOGI("MediaEngine thread started");
    
    while (running_.load() && !stopSource_.stop_requested()) {
        // Get current requests
        std::vector<ActiveClipRequest> videoRequests;
        std::vector<ActiveAudioRequest> audioRequests;
        std::vector<ActiveProxyRequest> proxyRequests;
        
        {
            std::lock_guard<std::mutex> lock(requestMutex_);
            videoRequests = pendingRequests_;
            audioRequests = pendingAudioRequests_;
            proxyRequests = pendingProxyRequests_;
        }
        
        // Process video requests
        std::vector<std::string> activePaths;
        for (const auto& req : videoRequests) {
            VideoDecoder* decoder = decoderPool_.Acquire(req.sourceFilePath, device_);
            if (decoder) {
                if (req.sourceTimeSeconds >= 0) {
                    decoder->SeekTo(req.sourceTimeSeconds);
                }
                activePaths.push_back(req.sourceFilePath);
                
                // Decode frame
                auto frame = decoder->DequeueFrame(device_);
                if (frame) {
                    frameCache_.Put(req.clipId, *frame);
                }
            }
        }
        
        // Process audio requests
        for (const auto& req : audioRequests) {
            auto clipData = audioCache_.GetAudio(req.clipId);
            if (!clipData) {
                // Load audio
                audioCache_.LoadAudio(req.clipId, req.sourceFilePath);
            }
        }
        
        // Process proxy requests
        for (const auto& req : proxyRequests) {
            // Proxy generation would happen here
            // For now, just decode at lower resolution
        }
        
        // Cleanup idle decoders
        decoderPool_.ReleaseIdleExcept(activePaths);
        
        // Evict old frames
        frameCache_.EvictOutsideWindow();
        proxyCache_.EvictOutsideWindow();
        
        // Sleep to avoid busy loop
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    LOGI("MediaEngine thread stopped");
}

void MediaEngine::DecodeAudioFile(const std::string& clipId, const std::string& filePath) {
    audioCache_.LoadAudio(clipId, filePath);
}

void MediaEngine::GenerateProxy(const ActiveProxyRequest& request) {
    // Proxy generation would decode at lower resolution and encode to proxy file
    // Implementation would use MediaCodec encoder
}

} // namespace vfx