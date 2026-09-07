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
                               const MediaEngine& mediaEngine, AudioEngine* audioEngine)
    : device_(device), renderGraph_(renderGraph), nodeGraph_(nodeGraph),
      timeline_(timeline), mediaEngine_(mediaEngine), audioEngine_(audioEngine) {
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
    
    CompileResult compileResult = renderGraph_.Compile(nodeGraph_, outputNode->nodeId);
    if (!compileResult.Ok()) {
        lastError_ = "Failed to compile render graph";
        running_.store(false);
        return false;
    }
    
    renderPlan_ = std::make_unique<CompileResult>(std::move(compileResult));
    
    // Start threads
    exportThread_ = std::jthread(&ExportPipeline::ExportThreadMain, this, stopSource_.get_token());
    videoEncodeThread_ = std::jthread(&ExportPipeline::VideoEncodeThreadMain, this, stopSource_.get_token());
    audioEncodeThread_ = std::jthread(&ExportPipeline::AudioEncodeThreadMain, this, stopSource_.get_token());
    muxThread_ = std::jthread(&ExportPipeline::MuxThreadMain, this, stopSource_.get_token());
    
    return true;
}

bool ExportPipeline::WaitForCompletion() {
    if (!running_.load()) return true;
    
    exportThread_.join();
    videoEncodeThread_.join();
    audioEncodeThread_.join();
    muxThread_.join();
    
    Cleanup();
    running_.store(false);
    return lastError_.empty();
}

void ExportPipeline::Cancel() {
    cancelled_.store(true);
    stopSource_.request_stop();
    queueCV_.notify_all();
}

ExportProgress ExportPipeline::GetProgress() const {
    return progress_;
}

void ExportPipeline::ExportThreadMain(std::stop_token stopToken) {
    auto frameDuration = 1.0 / config_.frameRate;
    auto totalFrames = totalFrames_;
    
    LOGI("Starting export: %llu frames at %.2f fps", static_cast<unsigned long long>(totalFrames), config_.frameRate);
    
    for (uint64_t frameIdx = 0; frameIdx < totalFrames && !stopToken.stop_requested() && !cancelled_.load(); ++frameIdx) {
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

void ExportPipeline::VideoEncodeThreadMain(std::stop_token stopToken) {
    if (!InitializeVideoEncoder()) {
        lastError_ = "Failed to initialize video encoder";
        running_.store(false);
        return;
    }
    
    while (!stopToken.stop_requested() && !cancelled_.load()) {
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

void ExportPipeline::AudioEncodeThreadMain(std::stop_token stopToken) {
    if (!InitializeAudioEncoder()) {
        lastError_ = "Failed to initialize audio encoder";
        running_.store(false);
        return;
    }
    
    double frameDuration = 1.0 / config_.frameRate;
    int64_t frameDurationUs = static_cast<int64_t>(frameDuration * 1'000'000.0);
    
    for (uint64_t frameIdx = 0; frameIdx < totalFrames_ && !stopToken.stop_requested() && !cancelled_.load(); ++frameIdx) {
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

void ExportPipeline::MuxThreadMain(std::stop_token stopToken) {
    if (!InitializeMuxer()) {
        lastError_ = "Failed to initialize muxer";
        running_.store(false);
        return;
    }
    
    // Muxer runs until both video and audio are done
    while (!stopToken.stop_requested() && !cancelled_.load()) {
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
    
    // Create input surface ONCE, not per frame
    videoInputSurface_ = AMediaCodec_createInputSurface(videoEncoder_);
    if (!videoInputSurface_) {
        LOGE("Failed to create video input surface");
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
    if (videoInputSurface_) {
        ANativeWindow_release(videoInputSurface_);
        videoInputSurface_ = nullptr;
    }
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
    if (!videoEncoder_ || !videoInputSurface_) return;
    
    // The actual rendering to the input surface happens in RenderFrame
    // Here we just dequeue the encoded output
    AMediaCodecBufferInfo info{};
    info.presentationTimeUs = static_cast<int64_t>(frame.timelineTime * 1'000'000.0);
    info.flags = 0;
    info.size = 0;
    info.offset = 0;
    
    ssize_t outIndex = AMediaCodec_dequeueOutputBuffer(videoEncoder_, &info, 10000);
    if (outIndex >= 0) {
        size_t outSize;
        uint8_t* outData = AMediaCodec_getOutputBuffer(videoEncoder_, outIndex, &outSize);
        if (outData && outSize > 0) {
            AMediaMuxer_writeSampleData(muxer_, videoTrackIndex_, outData, &info);
        }
        AMediaCodec_releaseOutputBuffer(videoEncoder_, outIndex, false);
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
    
    size_t sampleCount = chunk.samples.size();
    size_t bytesNeeded = sampleCount * sizeof(int16_t);
    if (bytesNeeded > bufSize) return;
    
    int16_t* int16Buf = reinterpret_cast<int16_t*>(buf);
    for (size_t i = 0; i < sampleCount; ++i) {
        float sample = std::clamp(chunk.samples[i], -1.0f, 1.0f);
        int16Buf[i] = static_cast<int16_t>(sample * 32767.0f);
    }
    
    AMediaCodecBufferInfo info{};
    info.presentationTimeUs = chunk.presentationTimeUs;
    info.flags = 0;
    info.size = static_cast<size_t>(bytesNeeded);
    info.offset = 0;
    
    AMediaCodec_queueInputBuffer(audioEncoder_, bufIndex, 0, static_cast<size_t>(bytesNeeded), 
                                 chunk.presentationTimeUs, 0);
    
    // Drain output
    ssize_t outIndex = AMediaCodec_dequeueOutputBuffer(audioEncoder_, &info, 10000);
    if (outIndex >= 0) {
        size_t outSize;
        uint8_t* outData = AMediaCodec_getOutputBuffer(audioEncoder_, outIndex, &outSize);
        if (outData && outSize > 0) {
            AMediaMuxer_writeSampleData(muxer_, audioTrackIndex_, outData, &info);
        }
        AMediaCodec_releaseOutputBuffer(audioEncoder_, outIndex, false);
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
    
    auto acquired = device_.CreateTexture(outputDesc);
    if (!acquired) return std::nullopt;
    
    TextureHandle outputTexture = acquired.value;
    
    // Execute render graph for this frame
    // We need a framebuffer with the output texture
    // The render graph will render to this texture
    if (device_.BeginFrame()) {
        auto plan = renderGraph_.Compile(nodeGraph_, "output");
        if (plan.Ok()) {
            renderGraph_.Execute(nodeGraph_, plan, timelineTime, &mediaEngine_, nullptr, audioEngine_);
        }
        device_.EndFrame();
    }
    
    FrameData frame;
    frame.texture = outputTexture;
    frame.timelineTime = timelineTime;
    frame.frameIndex = framesWritten_;
    
    return frame;
}

std::optional<ExportPipeline::AudioChunk> ExportPipeline::GetMixedAudio(double startTime, double endTime) {
    AudioChunk chunk;
    chunk.presentationTimeUs = static_cast<int64_t>(startTime * 1'000'000.0);
    
    if (audioEngine_) {
        auto mixed = audioEngine_->GetMixedAudio(startTime, endTime - startTime);
        if (!mixed.empty()) {
            chunk.samples = std::move(mixed);
            return chunk;
        }
    }
    
    // Fallback: iterate timeline clips and mix manually (stub)
    return chunk;
}

bool ExportPipeline::StartBatchExport(const std::vector<ExportConfig>& configs, ProgressCallback callback) {
    if (running_.load()) {
        lastError_ = "Export already in progress";
        return false;
    }
    if (configs.empty()) {
        lastError_ = "Empty batch export queue";
        return false;
    }
    
    batchQueue_ = configs;
    batchCurrentIndex_ = 0;
    batchCallback_ = std::move(callback);
    batchMode_ = true;
    batchTotalFrames_ = 0;
    batchFramesCompleted_ = 0;
    
    for (const auto& cfg : batchQueue_) {
        batchTotalFrames_ += static_cast<uint64_t>(cfg.duration * cfg.frameRate);
    }
    
    progress_.totalFrames = batchTotalFrames_;
    progress_.framesWritten = 0;
    progress_.progress = 0.0;
    progress_.currentOperation = "Starting batch export...";
    progress_.errorMessage.clear();
    
    running_.store(true);
    cancelled_.store(false);
    lastError_.clear();
    startTime_ = std::chrono::steady_clock::now();
    
    exportThread_ = std::jthread(&ExportPipeline::BatchExportThreadMain, this, stopSource_.get_token());
    return true;
}

void ExportPipeline::BatchExportThreadMain(std::stop_token stopToken) {
    LOGI("Starting batch export of %zu jobs", batchQueue_.size());
    
    for (size_t i = 0; i < batchQueue_.size() && !stopToken.stop_requested() && !cancelled_.load(); ++i) {
        batchCurrentIndex_ = i;
        const auto& cfg = batchQueue_[i];
        
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            progress_.currentOperation = "Exporting job " + std::to_string(i + 1) + " / " + std::to_string(batchQueue_.size()) + ": " + cfg.outputPath;
            if (batchCallback_) batchCallback_(progress_);
        }
        
        ExportConfig currentCfg = cfg;
        ProgressCallback wrappedCallback = [this, currentCfg, i](const ExportProgress& p) {
            if (batchCallback_) {
                ExportProgress batchProgress = p;
                batchProgress.framesWritten = batchFramesCompleted_ + p.framesWritten;
                batchProgress.progress = batchTotalFrames_ > 0 ? static_cast<double>(batchProgress.framesWritten) / static_cast<double>(batchTotalFrames_) : 0.0;
                auto elapsed = std::chrono::steady_clock::now() - startTime_;
                batchProgress.elapsedSeconds = std::chrono::duration<double>(elapsed).count();
                if (batchProgress.framesWritten > 0 && batchProgress.elapsedSeconds > 0) {
                    batchProgress.estimatedRemainingSeconds = 
                        batchProgress.elapsedSeconds * (batchTotalFrames_ - batchProgress.framesWritten) / batchProgress.framesWritten;
                }
                batchProgress.currentOperation = "Job " + std::to_string(i + 1) + "/" + std::to_string(batchQueue_.size()) + ": " + p.currentOperation;
                batchCallback_(batchProgress);
            }
        };
        
        if (!StartExport(currentCfg, wrappedCallback)) {
            LOGE("Batch export job %zu failed to start: %s", i, lastError_.c_str());
            continue;
        }
        
        WaitForCompletion();
        
        if (!lastError_.empty()) {
            LOGE("Batch export job %zu failed: %s", i, lastError_.c_str());
        }
        
        batchFramesCompleted_ += static_cast<uint64_t>(currentCfg.duration * currentCfg.frameRate);
        
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            progress_.framesWritten = batchFramesCompleted_;
            progress_.progress = batchTotalFrames_ > 0 ? static_cast<double>(batchFramesCompleted_) / static_cast<double>(batchTotalFrames_) : 0.0;
            if (batchCallback_) batchCallback_(progress_);
        }
    }
    
    running_.store(false);
    LOGI("Batch export completed: %zu jobs", batchQueue_.size());
}

} // namespace vfx