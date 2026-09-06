#pragma once
// Sections 4, 8, 9 — Phase 2. Video frames are decoded via AMediaCodec
// directly into an AImageReader that was created with
// AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, so the decoded buffer never
// touches CPU memory or gets RGB-converted on the CPU (Section 4's
// hard requirement). VulkanDevice::ImportHardwareBuffer (Phase 2 half of
// GraphicsDevice.h) wraps the resulting AHardwareBuffer as a VkImage with a
// VkSamplerYcbcrConversion attached, so YUV->RGB happens for free during
// texture sampling on the GPU.
//
// Threading (Section 14): this subsystem owns its own thread
// (MediaEngine::mediaThread_) so decode latency never stalls the engine
// thread's render loop. The engine thread only ever calls the non-blocking
// MediaEngine::TryGetFrame(); it never calls into VideoDecoder directly.

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "engine/core/GraphicsDevice.h"

struct AMediaExtractor;
struct AMediaCodec;
struct AImageReader;
struct AImage;

namespace vfx {

struct DecodedFrame {
    TextureHandle texture; // already backend-imported (Phase 2: via
                            // GraphicsDevice::ImportHardwareBuffer) — never a
                            // CPU-side RGB buffer per Section 4's "do not
                            // decode into CPU memory and RGB-convert" rule
    int64_t presentationTimeUs = 0;
    uint32_t width = 0, height = 0;

    // Kept alive as long as `texture` is sampled from: releasing the
    // AImage back to the reader would let the reader recycle/overwrite the
    // underlying AHardwareBuffer out from under an in-flight GPU read.
    // FrameCache owns this handle's lifetime; see FrameCache::EvictOutsideWindow.
    std::shared_ptr<AImage> ownedImage;
};

// One AMediaCodec instance around one source file/track. Section 8: "do not
// blindly create one decoder per clip" — MediaEngine owns a bounded set of
// these (see DecoderPool) and hands them out based on which clips are
// actually near the playhead, per Timeline::ActiveClipsAt.
class VideoDecoder {
public:
    explicit VideoDecoder(std::string filePath) : filePath_(std::move(filePath)) {}
    ~VideoDecoder();

    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;

    // Opens the extractor + configures AMediaCodec against `device` so
    // decoded buffers can be imported without a CPU copy. Concretely:
    // creates an AImageReader with AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE,
    // takes its ANativeWindow via AImageReader_getWindow, and configures
    // AMediaCodec's output surface to that window (COLOR_FormatSurface,
    // i.e. no ByteBuffer output path is ever touched).
    bool Open(GraphicsDevice& device);
    void Close();

    [[nodiscard]] bool IsOpen() const { return isOpen_; }
    [[nodiscard]] const std::string& FilePath() const { return filePath_; }

    // Seeks the underlying extractor to the nearest sync sample at or before
    // `sourceTimeSeconds` (Section 7's frame-accurate seeking requirement).
    // AMediaCodec buffers frames internally, so a seek must also flush the
    // codec (AMediaCodec_flush) or stale frames from before the seek point
    // would surface first.
    bool SeekTo(double sourceTimeSeconds);

    // Pumps the codec: feeds any available input buffers from the
    // extractor, then drains one ready output buffer (if any) and turns it
    // into a DecodedFrame by acquiring the corresponding AImage from the
    // reader and asking `device` to import its AHardwareBuffer. Non-blocking
    // (all AMediaCodec/AImageReader calls use a 0 timeout) — safe to call
    // every media-thread tick even when nothing is ready yet.
    std::optional<DecodedFrame> DequeueFrame(GraphicsDevice& device);

    [[nodiscard]] double LastDecodedTimeSeconds() const { return lastDecodedTimeUs_ / 1'000'000.0; }

private:
    bool PumpInput();      // feeds extractor samples into the codec's input queue
    bool DrainOutput(int64_t* outPtsUs); // returns true if an output buffer was released-to-render

    std::string filePath_;
    AMediaExtractor* extractor_ = nullptr;
    AMediaCodec* codec_ = nullptr;
    AImageReader* imageReader_ = nullptr;
    int videoTrackIndex_ = -1;
    uint32_t width_ = 0, height_ = 0;
    bool isOpen_ = false;
    bool inputEOS_ = false;
    int64_t lastDecodedTimeUs_ = -1;
    int64_t pendingSeekTimeUs_ = -1; // set by SeekTo, consumed by next PumpInput
};

struct DecoderPoolConfig {
    // Section 8: cap concurrent decoders by device capability rather than by
    // clip count. A safe conservative default; Phase 6 should derive this
    // from MediaCodecInfo.CodecCapabilities / getMaxSupportedInstances at
    // runtime instead of a constant.
    uint32_t maxConcurrentDecoders = 4;
};

// Hands out VideoDecoder instances for the clips Timeline reports as active,
// reclaiming decoders for clips that have scrolled out of the cache window
// (Section 9) rather than closing/reopening on every scrub tick. Only ever
// touched from the media thread — no internal locking.
class DecoderPool {
public:
    explicit DecoderPool(DecoderPoolConfig config) : config_(config) {}

    // Returns an opened decoder for `clipSourcePath`, opening a new one
    // (evicting the least-recently-used decoder first if at capacity) if
    // none exists yet.
    VideoDecoder* Acquire(const std::string& clipSourcePath, GraphicsDevice& device);

    // Eviction policy: closes any decoder not in `keepPaths` beyond the
    // pool's capacity margin. Called once per media-thread tick with the
    // current active-clip set so decoders for clips that scrolled off the
    // timeline get reclaimed instead of accumulating forever.
    void ReleaseIdleExcept(const std::vector<std::string>& keepPaths);

    void CloseAll();

private:
    DecoderPoolConfig config_;
    struct Entry {
        std::unique_ptr<VideoDecoder> decoder;
        int64_t lastUsedTickCounter = 0;
    };
    std::unordered_map<std::string, Entry> active_;
    int64_t tickCounter_ = 0;
};

struct FrameCacheConfig {
    size_t maxBytes = 256 * 1024 * 1024; // Section 9: "configurable memory limits"
    int framesAhead = 2;
    int framesBehind = 2;
};

// Section 9: caches frames around the playhead, prioritizing current, then
// neighboring, then transition-required frames. Backed by pooled GPU
// textures (via GraphicsDevice) rather than CPU buffers, consistent with
// "store decoded frames in GPU-compatible memory where possible". Populated
// by the media thread, read (TryGet-style, non-blocking) by the engine
// thread — guarded by a mutex since it crosses that boundary.
class FrameCache {
public:
    explicit FrameCache(FrameCacheConfig config) : config_(config) {}

    void SetPlayheadHint(double timelineSeconds) {
        std::lock_guard<std::mutex> lock(mutex_);
        playheadHint_ = timelineSeconds;
    }
    [[nodiscard]] double PlayheadHint() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return playheadHint_;
    }

    // Nearest-match lookup within a small epsilon window rather than exact
    // frame-index equality, since callers key by continuously-varying
    // source time (post trim/speed mapping), not a frame index.
    std::optional<DecodedFrame> Get(const std::string& clipId, double sourceTimeSeconds) const;
    void Put(const std::string& clipId, DecodedFrame frame);
    void EvictOutsideWindow(); // called once per media-thread tick using playheadHint_

    [[nodiscard]] size_t CurrentBytes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentBytes_;
    }

private:
    // Rough per-frame byte estimate for accounting against maxBytes; a real
    // implementation should ask GraphicsDevice for the texture's actual
    // allocation size (imported hardware buffers don't own a fixed Vulkan
    // allocation size the way a pooled texture does).
    static size_t EstimateBytes(const DecodedFrame& f) {
        return static_cast<size_t>(f.width) * f.height * 2; // YCbCr 4:2:0 ~= 1.5-2 bytes/px
    }

    mutable std::mutex mutex_;
    FrameCacheConfig config_;
    double playheadHint_ = 0.0;
    size_t currentBytes_ = 0;
    struct Entry {
        DecodedFrame frame;
        double sourceTimeSeconds = 0.0;
    };
    // Keyed by clipId -> ordered-by-time entries. A handful of frames per
    // active clip at most (framesAhead + framesBehind + 1), so linear scan
    // per clip is fine; no need for an intrusive LRU list at this scale.
    std::unordered_map<std::string, std::vector<Entry>> cache_;
};

// Request describing one clip the engine thread currently wants decoded
// frames for, refreshed every engine tick from Timeline::ActiveClipsAt().
struct ActiveClipRequest {
    std::string clipId;       // matches Node::nodeId for the VideoSource node
    std::string sourceFilePath;
    double sourceTimeSeconds = 0.0;
};

// Facade tying DecoderPool + FrameCache together behind a dedicated media
// thread (Section 14's "Media Thread" box). RenderGraph's VideoSource pass
// execution (Phase 3 wiring, see RenderGraph::ExecutePass) calls
// TryGetFrame() from the engine thread; everything else here runs on
// mediaThread_.
class MediaEngine {
public:
    MediaEngine(GraphicsDevice& device, DecoderPoolConfig decoderConfig, FrameCacheConfig cacheConfig)
        : device_(device), decoderPool_(decoderConfig), frameCache_(cacheConfig) {}
    ~MediaEngine() { Stop(); }

    void Start();
    void Stop();

    // Called once per engine tick (cheap: just swaps a vector under a
    // mutex). Replaces the full set of clips the media thread should be
    // servicing this frame.
    void SetActiveClips(std::vector<ActiveClipRequest> clips, double playheadTimelineSeconds);

    // Non-blocking. Returns the cached frame nearest `sourceTimeSeconds` for
    // `clipId` if one has been decoded, or std::nullopt if the media thread
    // hasn't produced one yet (caller should reuse the previous frame /
    // show nothing rather than stall waiting).
    [[nodiscard]] std::optional<DecodedFrame> TryGetFrame(const std::string& clipId,
                                                           double sourceTimeSeconds) const {
        return frameCache_.Get(clipId, sourceTimeSeconds);
    }

private:
    void MediaThreadMain();

    GraphicsDevice& device_;
    DecoderPool decoderPool_;
    FrameCache frameCache_;

    std::mutex requestMutex_;
    std::vector<ActiveClipRequest> pendingRequests_;

    std::thread mediaThread_;
    std::atomic<bool> running_{false};
};

} // namespace vfx
