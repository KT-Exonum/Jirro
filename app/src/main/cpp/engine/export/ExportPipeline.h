#pragma once
// Phase 6: Export Pipeline - Offline rendering and video encoding.
// Renders the composition at arbitrary speed/quality, independent of real-time playback.

#include <android/media/NdkMediaCodec.h>
#include <android/media/NdkMediaFormat.h>
#include <android/media/NdkMediaMuxer.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/GraphicsDevice.h"
#include "engine/core/Types.h"
#include "engine/graph/RenderGraph.h"
#include "engine/graph/Node.h"
#include "engine/timeline/Timeline.h"

namespace vfx {

// Export configuration
struct ExportConfig {
    std::string outputPath;           // Output file path (.mp4)
    uint32_t width = 1920;            // Output resolution
    uint32_t height = 1080;
    double frameRate = 30.0;          // Output frame rate
    double startTime = 0.0;           // Timeline range to export
    double endTime = 10.0;
    int bitrateMbps = 20;             // Video bitrate
    bool useHardwareEncoder = true;   // Use MediaCodec vs software
    std::string codec = "video/avc";  // video/avc, video/hevc, video/vp9
    int quality = 23;                 // CRF-like quality (lower = better)
    bool includeAudio = false;        // Not implemented yet
    int maxConcurrentFrames = 2;      // Parallel frame rendering
};

// Export progress callback
using ExportProgressCallback = std::function<void(double progress, const std::string& status)>;

// Export result
struct ExportResult {
    bool success = false;
    std::string errorMessage;
    double elapsedSeconds = 0.0;
    uint64_t framesEncoded = 0;
    size_t outputFileSize = 0;
    double averageFps = 0.0;
};

/**
 * ExportPipeline: Renders timeline to video file.
 * - Runs on dedicated thread pool
 * - Can render faster or slower than real-time
 * - Uses hardware encoder (MediaCodec) when available
 * - Supports arbitrary resolution/frame rate
 */
class ExportPipeline {
public:
    ExportPipeline(GraphicsDevice& device, const RenderGraph& renderGraph, 
                   const NodeGraph& nodeGraph, const Timeline& timeline);
    ~ExportPipeline();
    
    // Start export asynchronously
    // Returns immediately, calls progress callback during export
    void StartExport(const ExportConfig& config, ExportProgressCallback progressCb,
                     std::function<void(ExportResult)> completionCb);
    
    // Cancel ongoing export
    void Cancel();
    
    // Check if export is running
    [[nodiscard]] bool IsRunning() const;
    
    // Synchronous export (blocks until complete)
    ExportResult ExportSync(const ExportConfig& config);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * VideoEncoder: Hardware-accelerated video encoding via MediaCodec.
 * Wraps AMediaCodec for H.264/HEVC encoding from Vulkan/GPU frames.
 */
class VideoEncoder {
public:
    VideoEncoder();
    ~VideoEncoder();
    
    // Initialize encoder
    // width/height: output resolution
    // frameRate: frames per second
    // bitrateBps: target bitrate in bits per second
    // mime: "video/avc", "video/hevc", "video/vp9"
    bool Initialize(uint32_t width, uint32_t height, double frameRate,
                    int bitrateBps, const std::string& mime);
    
    // Encode a frame from GPU texture
    // texture: already in GPU memory (Vulkan image)
    // presentationTimeUs: timestamp in microseconds
    // Returns true if frame was accepted
    bool EncodeFrame(TextureHandle texture, int64_t presentationTimeUs);
    
    // Signal end of stream
    void SignalEndOfStream();
    
    // Drain encoded packets, write to muxer
    // muxer: AMediaMuxer to write to
    // trackIndex: video track index from muxer
    // Returns true if more packets available
    bool DrainOutput(AMediaMuxer* muxer, ssize_t trackIndex);
    
    // Flush any remaining frames
    void Flush();
    
    [[nodiscard]] bool IsInitialized() const { return initialized_; }

private:
    AMediaCodec* codec_ = nullptr;
    AMediaFormat* format_ = nullptr;
    bool initialized_ = false;
    bool eosSignaled_ = false;
    uint32_t width_ = 0, height_ = 0;
    int64_t frameIntervalUs_ = 0;
};

/**
 * MediaMuxer: Container writing (MP4/MOV)
 */
class MediaMuxer {
public:
    MediaMuxer();
    ~MediaMuxer();
    
    // Create muxer for output file
    // outputPath: file path (.mp4)
    // Returns true on success
    bool Initialize(const std::string& outputPath);
    
    // Add video track from encoder format
    // Returns track index or -1 on failure
    ssize_t AddVideoTrack(AMediaFormat* format);
    
    // Write sample data
    bool WriteSampleData(ssize_t trackIndex, const uint8_t* data, size_t size,
                         int64_t presentationTimeUs, uint32_t flags);
    
    // Finalize and close file
    bool Finalize();
    
    [[nodiscard]] bool IsInitialized() const { return initialized_; }

private:
    AMediaMuxer* muxer_ = nullptr;
    bool initialized_ = false;
};

} // namespace vfx