#include "ExportPipeline.h"

#include <android/log.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaMuxer.h>

#include <chrono>
#include <condition_variable>
#include <algorithm>
#include <mutex>
#include <queue>
#include <thread>

#define LOG_TAG "ExportPipeline"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vfx {

// --- VideoEncoder Implementation ---

VideoEncoder::VideoEncoder() = default;

VideoEncoder::~VideoEncoder() {
    if (codec_) {
        AMediaCodec_stop(codec_);
        AMediaCodec_delete(codec_);
    }
    if (format_) {
        AMediaFormat_delete(format_);
    }
}

bool VideoEncoder::Initialize(uint32_t width, uint32_t height, double frameRate,
                              int bitrateBps, const std::string& mime) {
    width_ = width;
    height_ = height;
    frameIntervalUs_ = static_cast<int64_t>(1'000'000.0 / frameRate);
    
    codec_ = AMediaCodec_createEncoderByType(mime.c_str());
    if (!codec_) {
        LOGE("Failed to create encoder for mime: %s", mime.c_str());
        return false;
    }
    
    format_ = AMediaFormat_new();
    AMediaFormat_setString(format_, AMEDIAFORMAT_KEY_MIME, mime.c_str());
    AMediaFormat_setInt32(format_, AMEDIAFORMAT_KEY_WIDTH, static_cast<int32_t>(width));
    AMediaFormat_setInt32(format_, AMEDIAFORMAT_KEY_HEIGHT, static_cast<int32_t>(height));
    AMediaFormat_setInt32(format_, AMEDIAFORMAT_KEY_BIT_RATE, bitrateBps);
    AMediaFormat_setInt32(format_, AMEDIAFORMAT_KEY_FRAME_RATE, static_cast<int32_t>(frameRate));
    AMediaFormat_setInt32(format_, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 30); // Keyframe every 30 frames
    AMediaFormat_setInt32(format_, AMEDIAFORMAT_KEY_COLOR_FORMAT, 2130708361); // COLOR_FormatSurface
    
    if (AMediaCodec_configure(codec_, format_, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE) != AMEDIA_OK) {
        LOGE("AMediaCodec_configure failed");
        return false;
    }
    
    // Create input surface for GPU frames
    ANativeWindow* inputSurface = nullptr;
    media_status_t status = AMediaCodec_createInputSurface(codec_, &inputSurface);
    if (status != AMEDIA_OK || !inputSurface) {
        LOGE("Failed to create input surface: %d", status);
        return false;
    }
    
    // Store surface for frame submission (in real impl, we'd use this)
    // For now, we'll note that we need to blit to this surface
    
    if (AMediaCodec_start(codec_) != AMEDIA_OK) {
        LOGE("AMediaCodec_start failed");
        return false;
    }
    
    initialized_ = true;
    eosSignaled_ = false;
    LOGI("VideoEncoder initialized: %ux%u @ %.1ffps, %d kbps, %s", width, height, frameRate, bitrateBps / 1000, mime.c_str());
    return true;
}

bool VideoEncoder::EncodeFrame(TextureHandle texture, int64_t presentationTimeUs) {
    if (!initialized_ || !codec_) return false;
    
    // In a real implementation:
    // 1. Get input surface from codec
    // 2. Blit the Vulkan texture to the input surface (via GPU blit)
    // 3. Signal frame availability to encoder
    
    // This is a simplified version - actual implementation needs:
    // - Vulkan->ANativeWindow blit (using VK_ANDROID_external_memory_android_hardware_buffer import in reverse)
    // - Or render directly to a surface backed by the codec's input surface
    
    LOGI("EncodeFrame: texture=%u, time=%lld us", texture.index, presentationTimeUs);
    return true;
}

void VideoEncoder::SignalEndOfStream() {
    if (!initialized_ || eosSignaled_) return;
    AMediaCodec_signalEndOfInputStream(codec_);
    eosSignaled_ = true;
}

bool VideoEncoder::DrainOutput(AMediaMuxer* muxer, ssize_t trackIndex) {
    if (!initialized_ || !muxer) return false;
    
    AMediaCodecBufferInfo info{};
    ssize_t outIndex = AMediaCodec_dequeueOutputBuffer(codec_, &info, 10000); // 10ms timeout
    
    if (outIndex >= 0) {
        size_t outSize = 0;
        uint8_t* outData = AMediaCodec_getOutputBuffer(codec_, outIndex, &outSize);
        if (outData && info.size > 0) {
            uint32_t flags = 0;
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) flags |= 1;
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_KEY_FRAME) flags |= 2;
            
            bool success = AMediaMuxer_writeSampleData(muxer, trackIndex, outData, info.size,
                                                        info.presentationTimeUs, flags) == AMEDIA_OK;
            AMediaCodec_releaseOutputBuffer(codec_, outIndex, false);
            return success;
        }
        AMediaCodec_releaseOutputBuffer(codec_, outIndex, false);
        return true; // Try again
    }
    
    return outIndex != AMEDIACODEC_INFO_TRY_AGAIN_LATER;
}

void VideoEncoder::Flush() {
    if (!initialized_ || !codec_) return;
    
    if (!eosSignaled_) {
        SignalEndOfStream();
    }
    
    // Drain remaining frames
    AMediaCodecBufferInfo info{};
    while (true) {
        ssize_t outIndex = AMediaCodec_dequeueOutputBuffer(codec_, &info, 10000);
        if (outIndex < 0) break;
        AMediaCodec_releaseOutputBuffer(codec_, outIndex, false);
        if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) break;
    }
}

// --- MediaMuxer Implementation ---

MediaMuxer::MediaMuxer() = default;

MediaMuxer::~MediaMuxer() {
    if (initialized_ && muxer_) {
        AMediaMuxer_stop(muxer_);
        AMediaMuxer_delete(muxer_);
    }
}

bool MediaMuxer::Initialize(const std::string& outputPath) {
    muxer_ = AMediaMuxer_new(outputPath.c_str(), AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
    if (!muxer_) {
        LOGE("Failed to create media muxer for: %s", outputPath.c_str());
        return false;
    }
    initialized_ = true;
    return true;
}

ssize_t MediaMuxer::AddVideoTrack(AMediaFormat* format) {
    if (!initialized_ || !muxer_ || !format) return -1;
    return AMediaMuxer_addTrack(muxer_, format);
}

bool MediaMuxer::WriteSampleData(ssize_t trackIndex, const uint8_t* data, size_t size,
                                 int64_t presentationTimeUs, uint32_t flags) {
    if (!initialized_ || !muxer_) return false;
    return AMediaMuxer_writeSampleData(muxer_, trackIndex, data, size, presentationTimeUs, flags) == AMEDIA_OK;
}

bool MediaMuxer::Finalize() {
    if (!initialized_ || !muxer_) return false;
    media_status_t status = AMediaMuxer_stop(muxer_);
    AMediaMuxer_delete(muxer_);
    muxer_ = nullptr;
    initialized_ = false;
    return status == AMEDIA_OK;
}

// --- ExportPipeline Implementation ---

struct ExportPipeline::Impl {
    GraphicsDevice& device;
    const RenderGraph& renderGraph;
    const NodeGraph& nodeGraph;
    const Timeline& timeline;
    
    std::thread exportThread;
    std::mutex mutex;
    std::condition_variable cv;
    bool running = false;
    bool cancelled = false;
    
    ExportConfig config;
    ExportProgressCallback progressCb;
    std::function<void(ExportResult)> completionCb;
    ExportResult result;
    
    std::unique_ptr<VideoEncoder> encoder;
    std::unique_ptr<MediaMuxer> muxer;
    ssize_t videoTrackIndex = -1;
    
    Impl(GraphicsDevice& d, const RenderGraph& rg, const NodeGraph& ng, const Timeline& tl)
        : device(d), renderGraph(rg), nodeGraph(ng), timeline(tl) {}
    
    void Run() {
        auto startTime = std::chrono::high_resolution_clock::now();
        result = ExportResult{};
        result.success = false;
        
        try {
            // Calculate frame count
            double duration = config.endTime - config.startTime;
            uint64_t totalFrames = static_cast<uint64_t>(duration * config.frameRate);
            if (totalFrames == 0) throw std::runtime_error("Zero frames to export");
            
            // Initialize encoder
            encoder = std::make_unique<VideoEncoder>();
            std::string mime;
            std::string muxerFormat;
            
            switch (config.format) {
                case ExportConfig::Format::MP4:
                    mime = config.codec;
                    muxerFormat = "mp4";
                    break;
                case ExportConfig::Format::MOV:
                    mime = "video/prores"; // ProRes
                    muxerFormat = "mov";
                    break;
                case ExportConfig::Format::WEBM:
                    mime = config.codec; // video/vp9 or video/av1
                    muxerFormat = "webm";
                    break;
                default:
                    mime = config.codec;
                    muxerFormat = "mp4";
            }
            
            // For non-video formats (GIF, PNG sequence), we don't use MediaCodec
            bool useVideoEncoder = (config.format != ExportConfig::Format::GIF &&
                                   config.format != ExportConfig::Format::PNG_SEQUENCE &&
                                   config.format != ExportConfig::Format::EXR_SEQUENCE);
            
            if (useVideoEncoder) {
                encoder = std::make_unique<VideoEncoder>();
                if (!encoder->Initialize(config.width, config.height, config.frameRate,
                                         config.bitrateMbps * 1'000'000, mime)) {
                    throw std::runtime_error("Failed to initialize encoder");
                }
            }
            
            // Initialize muxer for video formats
            if (config.format == ExportConfig::Format::MP4 ||
                config.format == ExportConfig::Format::MOV ||
                config.format == ExportConfig::Format::WEBM) {
                muxer = std::make_unique<MediaMuxer>();
                if (!muxer->Initialize(config.outputPath)) {
                    throw std::runtime_error("Failed to initialize muxer");
                }
                
                AMediaFormat* format = AMediaFormat_new();
                AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mime.c_str());
                AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, static_cast<int32_t>(config.width));
                AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, static_cast<int32_t>(config.height));
                AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, static_cast<int32_t>(config.frameRate));
                AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, config.bitrateMbps * 1'000'000);
                
                videoTrackIndex = muxer->AddVideoTrack(format);
                AMediaFormat_delete(format);
                
                if (videoTrackIndex < 0) {
                    throw std::runtime_error("Failed to add video track to muxer");
                }
            }
            
            // Find output node
            const std::string outputNodeId = "output";
            const Node* outputNode = nodeGraph.FindNode(outputNodeId);
            if (!outputNode) {
                throw std::runtime_error("Output node not found");
            }
            
            // Compile render graph
            auto plan = renderGraph.Compile(nodeGraph, outputNodeId);
            if (!plan.Ok()) {
                throw std::runtime_error("Failed to compile render graph");
            }
            
            // Render frames
            for (uint64_t frameIdx = 0; frameIdx < totalFrames && !cancelled; ++frameIdx) {
                double timelineTime = config.startTime + (frameIdx / config.frameRate);
                
                // Render frame at this timeline time
                renderGraph.Execute(nodeGraph, plan, timelineTime, nullptr);
                
                // Get output texture (in real impl, from renderGraph's final pass)
                // For now, we'd need to extract the final rendered texture
                
                // Encode frame
                int64_t ptsUs = static_cast<int64_t>(timelineTime * 1'000'000);
                // encoder->EncodeFrame(outputTexture, ptsUs);
                
                // Drain encoder output
                while (encoder->DrainOutput(muxer->muxer_, videoTrackIndex)) {
                    // Continue draining
                }
                
                // Update progress
                double progress = static_cast<double>(frameIdx) / totalFrames;
                if (progressCb) {
                    progressCb(progress, "Encoding frame " + std::to_string(frameIdx) + " / " + std::to_string(totalFrames));
                }
                
                result.framesEncoded++;
            }
            
            if (!cancelled) {
                // Flush encoder for video formats
                if (config.format == ExportConfig::Format::MP4 ||
                    config.format == ExportConfig::Format::MOV ||
                    config.format == ExportConfig::Format::WEBM) {
                    encoder->Flush();
                    while (encoder->DrainOutput(muxer->muxer_, videoTrackIndex)) {}
                    
                    // Finalize muxer
                    if (!muxer->Finalize()) {
                        throw std::runtime_error("Failed to finalize muxer");
                    }
                }
                
                result.success = true;
            }
            
        } catch (const std::exception& e) {
            result.errorMessage = e.what();
            LOGE("Export failed: %s", e.what());
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.elapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();
        result.averageFps = result.framesEncoded / std::max(result.elapsedSeconds, 0.001);
        
        // Get output file size
        // (would need platform-specific file stat)
        
        if (completionCb) {
            completionCb(result);
        }
        
        {
            std::lock_guard<std::mutex> lock(mutex);
            running = false;
        }
        cv.notify_all();
    }
};

ExportPipeline::ExportPipeline(GraphicsDevice& device, const RenderGraph& renderGraph,
                               const NodeGraph& nodeGraph, const Timeline& timeline)
    : impl_(std::make_unique<Impl>(device, renderGraph, nodeGraph, timeline)) {}

ExportPipeline::~ExportPipeline() {
    Cancel();
    if (impl_->exportThread.joinable()) {
        impl_->exportThread.join();
    }
}

void ExportPipeline::StartExport(const ExportConfig& config, ExportProgressCallback progressCb,
                                 std::function<void(ExportResult)> completionCb) {
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->running) return;
        impl_->config = config;
        impl_->progressCb = std::move(progressCb);
        impl_->completionCb = std::move(completionCb);
        impl_->running = true;
        impl_->cancelled = false;
    }
    
    impl_->exportThread = std::thread(&Impl::Run, impl_.get());
}

void ExportPipeline::Cancel() {
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->cancelled = true;
    }
    impl_->cv.notify_all();
}

bool ExportPipeline::IsRunning() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->running;
}

ExportResult ExportPipeline::ExportSync(const ExportConfig& config) {
    ExportResult result;
    std::mutex m;
    std::condition_variable cv;
    bool done = false;
    
    StartExport(config,
        [](double, const std::string&) {},
        [&](ExportResult r) {
            result = r;
            std::lock_guard<std::mutex> lock(m);
            done = true;
            cv.notify_one();
        });
    
    std::unique_lock<std::mutex> lock(m);
    cv.wait(lock, [&] { return done; });
    
    return result;
}

} // namespace vfx