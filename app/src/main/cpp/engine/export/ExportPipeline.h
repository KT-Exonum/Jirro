#pragma once
// Export pipeline: H.264/HEVC encoding via MediaCodec
// Renders timeline to video file with audio mixing

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <optional>
#include <functional>

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaMuxer.h>
#include <media/NdkMediaFormat.h>

#include "engine/core/GraphicsDevice.h"
#include "engine/graph/RenderGraph.h"
#include "engine/timeline/Timeline.h"
#include "engine/media/MediaEngine.h"

namespace vfx {

struct ExportConfig {
    std::string outputPath;
    uint32_t width = 1920;
    uint32_t height = 1080;
    double frameRate = 30.0;
    double duration = 10.0;
    
    // Video
    std::string videoCodec = "video/avc"; // "video/avc" (H.264) or "video/hevc" (H.265)
    int bitrateMbps = 20;
    int profile = 1; // H.264 High Profile
    int level = 0;
    int gopSize = 30;
    
    // Audio
    std::string audioCodec = "audio/mp4a-latm"; // AAC
    int audioBitrateKbps = 192;
    int audioSampleRate = 48000;
    int audioChannels = 2;
    
    // Quality
    bool hardwareAccelerated = true;
    bool constantFrameRate = true;
    
    // Output
    bool includeAlpha = false;
    std::string colorSpace = "bt709"; // bt709, bt2020, p3
    std::string colorRange = "limited"; // limited, full
};

struct ExportProgress {
    double progress = 0.0; // 0.0 - 1.0
    uint64_t framesWritten = 0;
    uint64_t totalFrames = 0;
    double elapsedSeconds = 0.0;
    double estimatedRemainingSeconds = 0.0;
    std::string currentOperation;
    std::string errorMessage;
};

using ProgressCallback = std::function<void(const ExportProgress&)>;

class ExportPipeline {
public:
    ExportPipeline(GraphicsDevice& device, const RenderGraph& renderGraph, 
                   const NodeGraph& nodeGraph, const Timeline& timeline,
                   const MediaEngine& mediaEngine);
    ~ExportPipeline();
    
    // Start async export
    bool StartExport(const ExportConfig& config, ProgressCallback callback = nullptr);
    
    // Wait for completion (blocking)
    bool WaitForCompletion();
    
    // Cancel in-progress export
    void Cancel();
    
    // Check status
    [[nodiscard]] bool IsRunning() const { return running_.load(); }
    [[nodiscard]] bool IsCancelled() const { return cancelled_.load(); }
    [[nodiscard]] ExportProgress GetProgress() const;
    
    // Get error if any
    [[nodiscard]] std::string GetError() const { return lastError_; }

private:
    struct FrameData {
        TextureHandle texture;
        double timelineTime = 0.0;
        uint64_t frameIndex = 0;
    };
    
    struct AudioChunk {
        std::vector<float> samples;
        int64_t presentationTimeUs = 0;
    };

    void ExportThreadMain();
    void VideoEncodeThreadMain();
    void AudioEncodeThreadMain();
    void MuxThreadMain();
    
    bool InitializeVideoEncoder();
    bool InitializeAudioEncoder();
    bool InitializeMuxer();
    void Cleanup();
    
    void EncodeVideoFrame(const FrameData& frame);
    void EncodeAudioChunk(const AudioChunk& chunk);
    void FlushEncoders();
    
    // Render a single frame at timeline time
    std::optional<FrameData> RenderFrame(double timelineTime);
    
    // Get mixed audio for time range
    std::optional<AudioChunk> GetMixedAudio(double startTime, double endTime);

    GraphicsDevice& device_;
    const RenderGraph& renderGraph_;
    const NodeGraph& nodeGraph_;
    const Timeline& timeline_;
    const MediaEngine& mediaEngine_;
    
    ExportConfig config_;
    ProgressCallback progressCallback_;
    
    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};
    std::string lastError_;
    
    std::thread exportThread_;
    std::thread videoEncodeThread_;
    std::thread audioEncodeThread_;
    std::thread muxThread_;
    
    // Synchronization
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
    std::queue<FrameData> frameQueue_;
    std::queue<AudioChunk> audioQueue_;
    std::atomic<bool> framesDone_{false};
    std::atomic<bool> audioDone_{false};
    
    // MediaCodec handles
    AMediaCodec* videoEncoder_ = nullptr;
    AMediaCodec* audioEncoder_ = nullptr;
    AMediaMuxer* muxer_ = nullptr;
    AMediaFormat* videoFormat_ = nullptr;
    AMediaFormat* audioFormat_ = nullptr;
    int videoTrackIndex_ = -1;
    int audioTrackIndex_ = -1;
    
    ExportProgress progress_;
    std::chrono::steady_clock::time_point startTime_;
    
    // Frame rendering
    std::unique_ptr<RenderGraph::CompileResult> renderPlan_;
    uint64_t totalFrames_ = 0;
    uint64_t framesWritten_ = 0;
};

} // namespace vfx