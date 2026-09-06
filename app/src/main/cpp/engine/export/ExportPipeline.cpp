#include "ExportPipeline.h"

#include <android/log.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaMuxer.h>
#include <media/NdkMediaFormat.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#define LOG_TAG "ExportPipeline"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vfx {

ExportPipeline::ExportPipeline(GraphicsDevice& device, const RenderGraph& renderGraph,
                               const NodeGraph& nodeGraph, const Timeline& timeline,
                               const MediaEngine& mediaEngine)
    : device_(device), renderGraph_(renderGraph), nodeGraph_(nodeGraph),
      timeline_(timeline), mediaEngine_(mediaEngine) {
}

ExportPipeline::~ExportPipeline() {
    Cancel();
    if (exportThread_.joinable()) exportThread_.join();
    if (videoEncodeThread_.joinable()) videoEncodeThread_.join();
    if (audioEncodeThread_.joinable()) audioEncodeThread_.join();
    if (muxThread_.joinable()) muxThread_.join();
    Cleanup();
}

bool ExportPipeline::StartExport(const ExportConfig& config, ProgressCallback callback) {
    if (running_.load()) {
        lastError_ = "Export already in progress";
        return false;
    }
    
    config_ = config;
    progressCallback_ = callback;
    running_.store(true);
    cancelled_.store(false);
    framesDone_.store(false);
    audioDone_.store(false);
    framesWritten_ = 0;
    lastError_.clear();
    startTime_ = std::chrono::steady_clock::now();
    
    totalFrames_ = static_cast<uint64_t>(config.duration * config.frameRate);
    progress_.totalFrames = totalFrames_;
    
    // Compile render graph
    // Find output node
    const Node* outputNode = nullptr;
    for (const auto& [id, node] : nodeGraph_.AllNodes()) {
        if (node.kind == NodeKind::Output) {
            outputNode = &node;
            break;
        }
    }
    if (!outputNode) {
        lastError_ = "No output node found in graph";
        running_.store(false);
        return false;
    }
    
    renderPlan_ = renderGraph_.Compile(nodeGraph_, outputNode->nodeId);
    if (!renderPlan_ || renderPlan_->passes.empty()) {
        lastError_ = "Failed to compile render graph";
        running_.store(false);
        return false;
    }
    
    // Start threads
    exportThread_ = std::thread(&ExportPipeline::ExportThreadMain, this);
    videoEncodeThread_ = std::thread(&ExportPipeline::VideoEncodeThreadMain, this);
    audioEncodeThread_ = std::thread(&ExportPipeline::AudioEncodeThreadMain, this);
    muxThread_ = std::thread(&ExportPipeline::MuxThreadMain, this);
    
    return true;
}

bool ExportPipeline::WaitForCompletion() {
    if (!running_.load()) return true;
    
    if (exportThread_.joinable()) exportThread_.join();
    if (videoEncodeThread_.joinable()) videoEncodeThread_.join();
    if (audioEncodeThread_.joinable()) audioEncodeThread_.join();
    if (muxThread_.joinable()) muxThread_.join();
    
    Cleanup();
    running_.store(false);
    return lastError_.empty();
}

void ExportPipeline::Cancel() {
    cancelled_.store(true);
    queueCV_.notify_all();
}

ExportProgress ExportPipeline::GetProgress() const {
    return progress_;
}

void ExportPipeline::ExportThreadMain() {
    auto frameDuration = 1.0 / config_.frameRate;
    auto totalFrames = totalFrames_;
    
    LOGI("Starting export: %lu frames at %.2f fps", totalFrames, config_.frameRate);
    
    for (uint64_t frameIdx = 0; frameIdx < totalFrames && running_.load() && !cancelled_.load(); ++frameIdx) {
        double timelineTime = frameIdx * frameDuration;
        
        // Update progress
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            progress_.framesWritten = frameIdx;
            progress_.progress = static_cast<double>(frameIdx) / totalFrames;
            auto elapsed = std::chrono::steady_clock::now() - startTime_;
            progress_.elapsedSeconds = std::chrono::duration<double>(elapsed).count();
            if (frameIdx > 0) {
                progress_.estimatedRemainingSeconds = 
                    progress_.elapsedSeconds * (totalFrames - frameIdx) / frameIdx;
            }
            progress_.currentOperation = "Rendering frame " + std::to_string(frameIdx + 1) + " / " + std::to_string(totalFrames);
            if (progressCallback_) progressCallback_(progress_);
        }
        
        // Render frame
        auto frame = RenderFrame(timelineTime);
        if (!frame) {
            if (cancelled_.load()) break;
            LOGE("Failed to render frame %lu", frameIdx);
            continue;
        }
        
        // Push to video queue
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCV_.wait(lock, [this] { return frameQueue_.size() < 30 || cancelled_.load() || !running_.load(); });
            if (cancelled_.load() || !running_.load()) break;
            frameQueue_.push(std::move(*frame));
        }
        queueCV_.notify_one();
    }
    
    framesDone_.store(true);
    queueCV_.notify_all();
    LOGI("Export thread finished");
}

void ExportPipeline::VideoEncodeThreadMain() {
    if (!InitializeVideoEncoder()) {
        lastError_ = "Failed to initialize video encoder";
        running_.store(false);
        return;
    }
    
    while (running_.load() && !cancelled_.load()) {
        FrameData frame;
        bool hasFrame = false;
        
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCV_.wait(lock, [this] { return !frameQueue_.empty() || framesDone_.load() || cancelled_.load(); });
            if (!frameQueue_.empty()) {
                frame = std::move(frameQueue_.front());
                frameQueue_.pop();
                hasFrame = true;
            }
        }
        queueCV_.notify_one();
        
        if (hasFrame) {
            EncodeVideoFrame(frame);
        }
        
        if (framesDone_.load() && frameQueue_.empty()) break;
    }
    
    FlushEncoders();
    audioDone_.store(true);
    queueCV_.notify_all();
    LOGI("Video encode thread finished");
}

void ExportPipeline::AudioEncodeThreadMain() {
    if (!InitializeAudioEncoder()) {
        lastError_ = "Failed to initialize audio encoder";
        running_.store(false);
        return;
    }
    
    double frameDuration = 1.0 / config_.frameRate;
    int64_t frameDurationUs = static_cast<int64_t>(frameDuration * 1'000'000.0);
    
    for (uint64_t frameIdx = 0; frameIdx < totalFrames_ && running_.load() && !cancelled_.load(); ++frameIdx) {
        double startTime = frameIdx * frameDuration;
        double endTime = startTime + frameDuration;
        
        auto audio = GetMixedAudio(startTime, endTime);
        if (audio) {
            EncodeAudioChunk(*audio);
        }
    }
    
    audioDone_.store(true);
    queueCV_.notify_all();
    LOGI("Audio encode thread finished");
}

void ExportPipeline::MuxThreadMain() {
    if (!InitializeMuxer()) {
        lastError_ = "Failed to initialize muxer";
        running_.store(false);
        return;
    }
    
    // Muxer runs until both video and audio are done
    while (running_.load() && !cancelled_.load()) {
        // MediaMuxer handles muxing automatically when we write sample data
        // Just wait for completion
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (framesDone_.load() && audioDone_.load()) break;
    }
    
    // Finalize
    if (muxer_) {
        AMediaMuxer_stop(muxer_);
    }
    
    LOGI("Mux thread finished");
}

bool ExportPipeline::InitializeVideoEncoder() {
    videoFormat_ = AMediaFormat_new();
    AMediaFormat_setString(videoFormat_, AMEDIAFORMAT_KEY_MIME, config_.videoCodec.c_str());
    AMediaFormat_setInt32(videoFormat_, AMEDIAFORMAT_KEY_WIDTH, static_cast<int32_t>(config_.width));
    AMediaFormat_setInt32(videoFormat_, AMEDIAFORMAT_KEY_HEIGHT, static_cast<int32_t>(config_.height));
    AMediaFormat_setInt32(videoFormat_, AMEDIAFORMAT_KEY_BIT_RATE, config_.bitrateMbps * 1'000'000);
    AMediaFormat_setInt32(videoFormat_, AMEDIAFORMAT_KEY_FRAME_RATE, static_cast<int32_t>(config_.frameRate));
    AMediaFormat_setInt32(videoFormat_, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, config_.gopSize);
    AMediaFormat_setInt32(videoFormat_, AMEDIAFORMAT_KEY_COLOR_FORMAT, 2130708361); // COLOR_FormatSurface
    
    if (config_.videoCodec == "video/hevc") {
        AMediaFormat_setInt32(videoFormat_, AMEDIAFORMAT_KEY_PROFILE, 1); // Main profile
    } else {
        AMediaFormat_setInt32(videoFormat_, AMEDIAFORMAT_KEY_PROFILE, config_.profile);
    }
    if (config_.level > 0) {
        AMediaFormat_setInt32(videoFormat_, AMEDIAFORMAT_KEY_LEVEL, config_.level);
    }
    
    videoEncoder_ = AMediaCodec_createEncoderByType(config_.videoCodec.c_str());
    if (!videoEncoder_) {
        LOGE("Failed to create video encoder for %s", config_.videoCodec.c_str());
        return false;
    }
    
    if (AMediaCodec_configure(videoEncoder_, videoFormat_, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE) != AMEDIA_OK) {
        LOGE("Failed to configure video encoder");
        return false;
    }
    
    if (AMediaCodec_start(videoEncoder_) != AMEDIA_OK) {
        LOGE("Failed to start video encoder");
        return false;
    }
    
    return true;
}

bool ExportPipeline::InitializeAudioEncoder() {
    audioFormat_ = AMediaFormat_new();
    AMediaFormat_setString(audioFormat_, AMEDIAFORMAT_KEY_MIME, config_.audioCodec.c_str());
    AMediaFormat_setInt32(audioFormat_, AMEDIAFORMAT_KEY_SAMPLE_RATE, config_.audioSampleRate);
    AMediaFormat_setInt32(audioFormat_, AMEDIAFORMAT_KEY_CHANNEL_COUNT, config_.audioChannels);
    AMediaFormat_setInt32(audioFormat_, AMEDIAFORMAT_KEY_BIT_RATE, config_.audioBitrateKbps * 1000);
    AMediaFormat_setInt32(audioFormat_, AMEDIAFORMAT_KEY_AAC_PROFILE, 2); // AAC LC
    
    audioEncoder_ = AMediaCodec_createEncoderByType(config_.audioCodec.c_str());
    if (!audioEncoder_) {
        LOGE("Failed to create audio encoder");
        return false;
    }
    
    if (AMediaCodec_configure(audioEncoder_, audioFormat_, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE) != AMEDIA_OK) {
        LOGE("Failed to configure audio encoder");
        return false;
    }
    
    if (AMediaCodec_start(audioEncoder_) != AMEDIA_OK) {
        LOGE("Failed to start audio encoder");
        return false;
    }
    
    return true;
}

bool ExportPipeline::InitializeMuxer() {
    muxer_ = AMediaMuxer_new(config_.outputPath.c_str(), AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
    if (!muxer_) {
        LOGE("Failed to create muxer for %s", config_.outputPath.c_str());
        return false;
    }
    
    videoTrackIndex_ = AMediaMuxer_addTrack(muxer_, videoFormat_);
    audioTrackIndex_ = AMediaMuxer_addTrack(muxer_, audioFormat_);
    
    if (AMediaMuxer_start(muxer_) != AMEDIA_OK) {
        LOGE("Failed to start muxer");
        return false;
    }
    
    return true;
}

void ExportPipeline::Cleanup() {
    if (videoEncoder_) {
        AMediaCodec_stop(videoEncoder_);
        AMediaCodec_delete(videoEncoder_);
        videoEncoder_ = nullptr;
    }
    if (audioEncoder_) {
        AMediaCodec_stop(audioEncoder_);
        AMediaCodec_delete(audioEncoder_);
        audioEncoder_ = nullptr;
    }
    if (muxer_) {
        AMediaMuxer_delete(muxer_);
        muxer_ = nullptr;
    }
    if (videoFormat_) {
        AMediaFormat_delete(videoFormat_);
        videoFormat_ = nullptr;
    }
    if (audioFormat_) {
        AMediaFormat_delete(audioFormat_);
        audioFormat_ = nullptr;
    }
}

void ExportPipeline::EncodeVideoFrame(const FrameData& frame) {
    if (!videoEncoder_) return;
    
    // Get input buffer
    ssize_t bufIndex = AMediaCodec_dequeueInputBuffer(videoEncoder_, 10000);
    if (bufIndex < 0) return;
    
    // Get output surface (we render to this)
    ANativeWindow* surface = AMediaCodec_createInputSurface(videoEncoder_);
    if (!surface) return;
    
    // Render frame to surface (would use the graphics device to draw to the surface)
    // For now, we just signal the frame is ready
    AMediaCodecBufferInfo info{};
    info.presentationTimeUs = static_cast<int64_t>(frame.timelineTime * 1'000'000.0);
    info.flags = 0;
    info.size = 0;
    info.offset = 0;
    
    AMediaCodec_releaseOutputBuffer(videoEncoder_, bufIndex, true);
    
    // The surface rendering happens separately - we'd draw the frame to the surface
    // using the graphics device
    
    // For the muxer, we need the encoded data
    ssize_t outIndex = AMediaCodec_dequeueOutputBuffer(videoEncoder_, nullptr, 10000);
    if (outIndex >= 0) {
        AMediaCodecBufferInfo outInfo{};
        ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(videoEncoder_, &outInfo, 10000);
        if (outIdx >= 0) {
            size_t outSize;
            uint8_t* outData = AMediaCodec_getOutputBuffer(videoEncoder_, outIdx, &outSize);
            if (outData && outSize > 0) {
                AMediaMuxer_writeSampleData(muxer_, videoTrackIndex_, outData, &outInfo);
            }
            AMediaCodec_releaseOutputBuffer(videoEncoder_, outIdx, false);
        }
    }
    
    framesWritten_++;
}

void ExportPipeline::EncodeAudioChunk(const AudioChunk& chunk) {
    if (!audioEncoder_) return;
    
    ssize_t bufIndex = AMediaCodec_dequeueInputBuffer(audioEncoder_, 10000);
    if (bufIndex < 0) return;
    
    size_t bufSize;
    uint8_t* buf = AMediaCodec_getInputBuffer(audioEncoder_, bufIndex, &bufSize);
    if (!buf) return;
    
    // Convert float samples to int16 (AAC expects 16-bit PCM typically, but MediaCodec handles conversion)
    // For AAC encoder, we write float samples directly
    size_t sampleCount = chunk.samples.size();
    size_t bytesNeeded = sampleCount * sizeof(float);
    if (bytesNeeded > bufSize) return;
    
    std::memcpy(buf, chunk.samples.data(), bytesNeeded);
    
    AMediaCodecBufferInfo info{};
    info.presentationTimeUs = chunk.presentationTimeUs;
    info.flags = 0;
    info.size = static_cast<size_t>(bytesNeeded);
    info.offset = 0;
    
    AMediaCodec_queueInputBuffer(audioEncoder_, bufIndex, 0, static_cast<size_t>(bytesNeeded), 
                                 chunk.presentationTimeUs, 0);
    
    // Drain output
    ssize_t outIndex = AMediaCodec_dequeueOutputBuffer(audioEncoder_, nullptr, 10000);
    if (outIndex >= 0) {
        AMediaCodecBufferInfo outInfo{};
        ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(audioEncoder_, &outInfo, 10000);
        if (outIdx >= 0) {
            size_t outSize;
            uint8_t* outData = AMediaCodec_getOutputBuffer(audioEncoder_, outIdx, &outSize);
            if (outData && outSize > 0) {
                AMediaMuxer_writeSampleData(muxer_, audioTrackIndex_, outData, &outInfo);
            }
            AMediaCodec_releaseOutputBuffer(audioEncoder_, outIdx, false);
        }
    }
}

void ExportPipeline::FlushEncoders() {
    // Flush video encoder
    if (videoEncoder_) {
        AMediaCodec_signalEndOfInputStream(videoEncoder_);
        while (true) {
            AMediaCodecBufferInfo info{};
            ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(videoEncoder_, &info, 10000);
            if (outIdx < 0) break;
            if (info.size > 0) {
                size_t outSize;
                uint8_t* outData = AMediaCodec_getOutputBuffer(videoEncoder_, outIdx, &outSize);
                if (outData && outSize > 0) {
                    AMediaMuxer_writeSampleData(muxer_, videoTrackIndex_, outData, &info);
                }
            }
            AMediaCodec_releaseOutputBuffer(videoEncoder_, outIdx, false);
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) break;
        }
    }
    
    // Flush audio encoder
    if (audioEncoder_) {
        AMediaCodec_signalEndOfInputStream(audioEncoder_);
        while (true) {
            AMediaCodecBufferInfo info{};
            ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(audioEncoder_, &info, 10000);
            if (outIdx < 0) break;
            if (info.size > 0) {
                size_t outSize;
                uint8_t* outData = AMediaCodec_getOutputBuffer(audioEncoder_, outIdx, &outSize);
                if (outData && outSize > 0) {
                    AMediaMuxer_writeSampleData(muxer_, audioTrackIndex_, outData, &info);
                }
            }
            AMediaCodec_releaseOutputBuffer(audioEncoder_, outIdx, false);
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) break;
        }
    }
}

std::optional<ExportPipeline::FrameData> ExportPipeline::RenderFrame(double timelineTime) {
    if (!renderPlan_) return std::nullopt;
    
    // Create output texture for this frame
    TextureDesc outputDesc;
    outputDesc.width = config_.width;
    outputDesc.height = config_.height;
    outputDesc.format = PixelFormat::RGBA8Unorm;
    outputDesc.usage = TextureUsage::ColorAttachmentAndSampled;
    outputDesc.transient = true;
    outputDesc.debugName = "export_frame_" + std::to_string(framesWritten_);
    
    auto acquired = device_.texturePool().Acquire(outputDesc);
    if (!acquired) return std::nullopt;
    
    TextureHandle outputTexture = acquired.value;
    
    // Execute render graph for this frame
    // We need to set the timeline time for the render graph
    // This would involve updating uniform buffers with the current timeline time
    // and executing each pass in the render plan
    
    // For now, return a placeholder
    FrameData frame;
    frame.texture = outputTexture;
    frame.timelineTime = timelineTime;
    frame.frameIndex = framesWritten_;
    
    return frame;
}

std::optional<ExportPipeline::AudioChunk> ExportPipeline::GetMixedAudio(double startTime, double endTime) {
    // Get all active audio clips from timeline
    // Mix their audio samples
    // Return mixed audio chunk
    
    AudioChunk chunk;
    chunk.presentationTimeUs = static_cast<int64_t>(startTime * 1'000'000.0);
    
    // Placeholder: would mix audio from active clips
    // For now return empty
    return chunk;
}

} // namespace vfx