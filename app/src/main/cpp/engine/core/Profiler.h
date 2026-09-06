#pragma once
// Phase 6: Profiling, thermal adaptation, and device capability management.
// Provides GPU timestamp queries, CPU timing, memory tracking, and automatic
// quality scaling based on thermal state and performance targets.

#include <android/thermal.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace vfx {

// GPU timestamp query result
struct GpuTimestamp {
    uint64_t startNs = 0;
    uint64_t endNs = 0;
    bool valid = false;
    
    double DurationMs() const { return (endNs - startNs) / 1'000'000.0; }
};

// CPU timing scope (RAII)
class CpuTimer {
public:
    explicit CpuTimer(std::function<void(double)> onComplete) 
        : onComplete_(std::move(onComplete)), start_(std::chrono::high_resolution_clock::now()) {}
    
    ~CpuTimer() {
        if (onComplete_) {
            auto end = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(end - start_).count();
            onComplete_(ms);
        }
    }
    
private:
    std::function<void(double)> onComplete_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

// Aggregated frame statistics
struct ProfileFrameStats {
    // GPU timing
    double gpuTotalMs = 0.0;
    double gpuGraphicsMs = 0.0;
    double gpuComputeMs = 0.0;
    
    // CPU timing
    double cpuTotalMs = 0.0;
    double cpuRecordMs = 0.0;
    double cpuSubmitMs = 0.0;
    
    // Draw stats
    uint32_t drawCalls = 0;
    uint32_t pipelineBinds = 0;
    uint32_t descriptorSetBinds = 0;
    uint32_t renderPasses = 0;
    
    // Memory
    size_t textureMemoryBytes = 0;
    size_t bufferMemoryBytes = 0;
    size_t textureCount = 0;
    size_t bufferCount = 0;
    
    // Thermal
    int thermalStatus = 0; // AthermalManager status
    bool throttling = false;
    
    // Timing
    uint64_t frameIndex = 0;
    double frameTimeMs = 0.0;
    double fps = 0.0;
};

// Profiling configuration
struct ProfilerConfig {
    bool enableGpuTimestamps = true;
    bool enableCpuTiming = true;
    bool enableMemoryTracking = true;
    uint32_t timestampQueryPoolSize = 64; // Must be power of 2
    double targetFrameTimeMs = 16.67; // 60 FPS target
};

// Thermal state callback
using ThermalCallback = std::function<void(int /*thermalStatus*/)>;

// Forward declaration for VulkanDevice
class VulkanDevice;

/**
 * Profiler: collects GPU/CPU timing, memory usage, and thermal state.
 * Designed for minimal overhead - timestamp queries are batched and
 * results retrieved asynchronously (2-frame latency).
 */
class Profiler {
public:
    explicit Profiler(ProfilerConfig config = {});
    ~Profiler();
    
    // Initialize with Vulkan device (must be called after device creation)
    bool Initialize(VulkanDevice* device);
    void Shutdown();
    
    // Frame lifecycle
    void BeginFrame(uint64_t frameIndex);
    void EndFrame();
    
    // GPU timestamp queries (call between Begin/EndFrame)
    // Returns query index for EndGpuTimestamp
    uint32_t BeginGpuTimestamp(const char* label);
    void EndGpuTimestamp(uint32_t queryIndex);
    
    // CPU timing scopes
    [[nodiscard]] std::unique_ptr<CpuTimer> CpuScope(const char* label);
    
    // Memory tracking
    void TrackTextureAllocation(size_t bytes);
    void TrackTextureDeallocation(size_t bytes);
    void TrackBufferAllocation(size_t bytes);
    void TrackBufferDeallocation(size_t bytes);
    
    // Draw call tracking
    void RecordDrawCall();
    void RecordPipelineBind();
    void RecordDescriptorSetBind();
    void RecordRenderPass();
    
    // Get latest frame stats (thread-safe)
    ProfileFrameStats GetLastFrameStats() const;
    
    // Get rolling average stats over N frames
    ProfileFrameStats GetAverageStats(uint32_t frameCount = 60) const;
    
    // Thermal monitoring
    void SetThermalCallback(ThermalCallback cb);
    void UpdateThermalStatus();
    int GetThermalStatus() const { return thermalStatus_; }
    bool IsThrottling() const { return thermalStatus_ >= ATHERMAL_STATUS_WARNING; }
    
    // Performance analysis
    struct PerformanceReport {
        double avgFrameTimeMs = 0.0;
        double avgGpuTimeMs = 0.0;
        double avgCpuTimeMs = 0.0;
        double p99FrameTimeMs = 0.0;
        uint32_t framesAnalyzed = 0;
        bool gpuBound = false;
        bool cpuBound = false;
        std::string recommendation;
    };
    PerformanceReport GenerateReport(uint32_t frameCount = 120) const;
    
    // Export stats as JSON for UI
    std::string ExportStatsJson() const;

private:
    struct FrameData {
        ProfileFrameStats stats;
        std::vector<std::pair<std::string, GpuTimestamp>> gpuTimestamps;
        std::vector<std::pair<std::string, double>> cpuTimings;
    };
    
    ProfilerConfig config_;
    VulkanDevice* device_ = nullptr;
    
    // Timestamp query pool (Vulkan)
    // We'll implement this in the .cpp with Vulkan specifics
    class TimestampQueryPool;
    std::unique_ptr<TimestampQueryPool> timestampPool_;
    
    // Frame history (ring buffer)
    std::vector<FrameData> frameHistory_;
    uint32_t historySize_ = 120;
    uint32_t currentFrame_ = 0;
    mutable std::mutex historyMutex_;
    
    // Thermal monitoring
    AThermalManager* thermalManager_ = nullptr;
    ThermalCallback thermalCallback_;
    int thermalStatus_ = ATHERMAL_STATUS_NONE;
    
    // Current frame being recorded
    FrameData* currentFrameData_ = nullptr;
    uint64_t currentFrameIndex_ = 0;
    
    // Running totals for current frame
    double currentGpuMs_ = 0.0;
    double currentCpuMs_ = 0.0;
};

} // namespace vfx