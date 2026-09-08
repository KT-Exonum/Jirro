#include "Profiler.h"

#include <android/log.h>
#include <android/thermal.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <thread>
#include <cstring>

#define LOG_TAG "Profiler"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vfx {

// TimestampQueryPool implementation
class Profiler::TimestampQueryPool {
public:
    TimestampQueryPool(VulkanDevice* device, uint32_t size)
        : device_(device) {
        VkQueryPoolCreateInfo createInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        createInfo.queryCount = size;
        createInfo.pipelineStatistics = 0;
        
        VkResult result = vkCreateQueryPool(device_->RawDevice(), &createInfo, nullptr, &pool_);
        if (result != VK_SUCCESS) {
            LOGE("Failed to create timestamp query pool: %d", result);
            pool_ = VK_NULL_HANDLE;
        }
        
        // Get timestamp period for ns conversion
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(device_->PhysicalDevice(), &props);
        timestampPeriod_ = props.limits.timestampPeriod;
    }
    
    ~TimestampQueryPool() {
        if (pool_ != VK_NULL_HANDLE) {
            vkDestroyQueryPool(device_->RawDevice(), pool_, nullptr);
        }
    }
    
    [[nodiscard]] VkQueryPool Pool() const { return pool_; }
    [[nodiscard]] float TimestampPeriod() const { return timestampPeriod_; }
    [[nodiscard]] uint32_t Size() const { return size_; }
    
    void Reset(uint32_t firstQuery, uint32_t queryCount) {
        vkCmdResetQueryPool(device_->CurrentCommandBuffer(), pool_, firstQuery, queryCount);
    }
    
    // Get results (blocking)
    bool GetResults(uint32_t firstQuery, uint32_t queryCount, uint64_t* data) {
        VkResult result = vkGetQueryPoolResults(
            device_->RawDevice(),
            pool_,
            firstQuery,
            queryCount,
            queryCount * sizeof(uint64_t),
            data,
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );
        return result == VK_SUCCESS;
    }

private:
    VulkanDevice* device_;
    VkQueryPool pool_ = VK_NULL_HANDLE;
    float timestampPeriod_ = 1.0f;
    uint32_t size_ = 0;
};

Profiler::Profiler(ProfilerConfig config) : config_(std::move(config)) {
    frameHistory_.reserve(config_.timestampQueryPoolSize * 2);
    for (uint32_t i = 0; i < config_.timestampQueryPoolSize * 2; ++i) {
        frameHistory_.emplace_back();
    }
}

Profiler::~Profiler() {
    Shutdown();
}

bool Profiler::Initialize(VulkanDevice* device) {
    device_ = device;
    
    if (config_.enableGpuTimestamps) {
        timestampPool_ = std::make_unique<TimestampQueryPool>(device_, config_.timestampQueryPoolSize);
        if (!timestampPool_ || timestampPool_->Pool() == VK_NULL_HANDLE) {
            LOGE("Failed to initialize timestamp query pool");
            config_.enableGpuTimestamps = false;
        }
    }
    
    // Initialize thermal manager (API 30+)
#if __ANDROID_API__ >= 30
    thermalManager_ = AThermal_acquireManager();
    if (!thermalManager_) {
        LOGI("AThermal_acquireManager returned null - thermal monitoring unavailable");
    }
#else
    LOGI("Thermal monitoring requires API 30+, skipping on this device");
#endif
    
    LOGI("Profiler initialized: GPU timestamps=%s, CPU timing=%s, Memory tracking=%s",
         config_.enableGpuTimestamps ? "on" : "off",
         config_.enableCpuTiming ? "on" : "off",
         config_.enableMemoryTracking ? "on" : "off");
    
    return true;
}

void Profiler::Shutdown() {
#if __ANDROID_API__ >= 30
    if (thermalManager_) {
        AThermal_releaseManager(thermalManager_);
        thermalManager_ = nullptr;
    }
#endif
    timestampPool_.reset();
    device_ = nullptr;
}

void Profiler::BeginFrame(uint64_t frameIndex) {
    currentFrameIndex_ = frameIndex;
    currentFrameData_ = &frameHistory_[frameIndex % frameHistory_.size()];
    currentFrameData_->stats = ProfileFrameStats{}; // Reset
    currentFrameData_->stats.frameIndex = frameIndex;
    currentFrameData_->gpuTimestamps.clear();
    currentFrameData_->cpuTimings.clear();
    
    currentGpuMs_ = 0.0;
    currentCpuMs_ = 0.0;
    
    // Update thermal status
    UpdateThermalStatus();
    currentFrameData_->stats.thermalStatus = thermalStatus_;
    currentFrameData_->stats.throttling = IsThrottling();
}

void Profiler::EndFrame() {
    if (!currentFrameData_) return;
    
    // Sum up GPU timestamps
    for (const auto& [label, ts] : currentFrameData_->gpuTimestamps) {
        if (ts.valid) {
            currentFrameData_->stats.gpuTotalMs += ts.DurationMs();
            if (!label.empty() && std::strstr(label.c_str(), "Compute")) {
                currentFrameData_->stats.gpuComputeMs += ts.DurationMs();
            } else {
                currentFrameData_->stats.gpuGraphicsMs += ts.DurationMs();
            }
        }
    }
    
    // Sum up CPU timings
    for (const auto& [label, ms] : currentFrameData_->cpuTimings) {
        currentFrameData_->stats.cpuTotalMs += ms;
    }
    
    // Frame time
    if (!currentFrameData_->cpuTimings.empty()) {
        currentFrameData_->stats.frameTimeMs = currentFrameData_->stats.cpuTotalMs;
        currentFrameData_->stats.fps = 1000.0 / currentFrameData_->stats.frameTimeMs;
    }
    
    currentFrameData_ = nullptr;
}

uint32_t Profiler::BeginGpuTimestamp(const char* label) {
    if (!config_.enableGpuTimestamps || !timestampPool_ || !device_) return UINT32_MAX;
    
    uint32_t queryIndex = currentFrameIndex_ % timestampPool_->Size();
    
    // Reset this query
    timestampPool_->Reset(queryIndex, 1);
    
    // Write timestamp at bottom of pipe
    // Note: This should be called from within a command buffer recording
    // The actual vkCmdWriteTimestamp calls are made in VulkanDevice
    
    currentFrameData_->gpuTimestamps.emplace_back(label, GpuTimestamp{});
    return queryIndex;
}

void Profiler::EndGpuTimestamp(uint32_t queryIndex) {
    // The actual timestamp write happens in VulkanDevice command buffer
    // This just marks the query as having an end point
    // Results are read asynchronously in a later frame
}

std::unique_ptr<CpuTimer> Profiler::CpuScope(const char* label) {
    if (!config_.enableCpuTiming) return nullptr;
    
    return std::make_unique<CpuTimer>([this, label](double ms) {
        if (currentFrameData_) {
            currentFrameData_->cpuTimings.emplace_back(label, ms);
        }
    });
}

void Profiler::TrackTextureAllocation(size_t bytes) {
    if (!config_.enableMemoryTracking || !currentFrameData_) return;
    currentFrameData_->stats.textureMemoryBytes += bytes;
    currentFrameData_->stats.textureCount++;
}

void Profiler::TrackTextureDeallocation(size_t bytes) {
    if (!config_.enableMemoryTracking || !currentFrameData_) return;
    if (currentFrameData_->stats.textureMemoryBytes >= bytes) {
        currentFrameData_->stats.textureMemoryBytes -= bytes;
    }
    if (currentFrameData_->stats.textureCount > 0) {
        currentFrameData_->stats.textureCount--;
    }
}

void Profiler::TrackBufferAllocation(size_t bytes) {
    if (!config_.enableMemoryTracking || !currentFrameData_) return;
    currentFrameData_->stats.bufferMemoryBytes += bytes;
    currentFrameData_->stats.bufferCount++;
}

void Profiler::TrackBufferDeallocation(size_t bytes) {
    if (!config_.enableMemoryTracking || !currentFrameData_) return;
    if (currentFrameData_->stats.bufferMemoryBytes >= bytes) {
        currentFrameData_->stats.bufferMemoryBytes -= bytes;
    }
    if (currentFrameData_->stats.bufferCount > 0) {
        currentFrameData_->stats.bufferCount--;
    }
}

void Profiler::RecordDrawCall() {
    if (currentFrameData_) currentFrameData_->stats.drawCalls++;
}

void Profiler::RecordPipelineBind() {
    if (currentFrameData_) currentFrameData_->stats.pipelineBinds++;
}

void Profiler::RecordDescriptorSetBind() {
    if (currentFrameData_) currentFrameData_->stats.descriptorSetBinds++;
}

void Profiler::RecordRenderPass() {
    if (currentFrameData_) currentFrameData_->stats.renderPasses++;
}

ProfileFrameStats Profiler::GetLastFrameStats() const {
    std::lock_guard<std::mutex> lock(historyMutex_);
    if (frameHistory_.empty()) return ProfileFrameStats{};
    
    uint32_t idx = (currentFrameIndex_ + frameHistory_.size() - 1) % frameHistory_.size();
    return frameHistory_[idx].stats;
}

ProfileFrameStats Profiler::GetAverageStats(uint32_t frameCount) const {
    std::lock_guard<std::mutex> lock(historyMutex_);
    ProfileFrameStats avg{};
    uint32_t count = 0;
    
    uint32_t frames = std::min(frameCount, static_cast<uint32_t>(frameHistory_.size()));
    for (uint32_t i = 0; i < frames; ++i) {
        uint32_t idx = (currentFrameIndex_ + frameHistory_.size() - 1 - i) % frameHistory_.size();
        const auto& stats = frameHistory_[idx].stats;
        if (stats.frameIndex == 0) continue;
        
        avg.gpuTotalMs += stats.gpuTotalMs;
        avg.gpuGraphicsMs += stats.gpuGraphicsMs;
        avg.gpuComputeMs += stats.gpuComputeMs;
        avg.cpuTotalMs += stats.cpuTotalMs;
        avg.cpuRecordMs += stats.cpuRecordMs;
        avg.cpuSubmitMs += stats.cpuSubmitMs;
        avg.drawCalls += stats.drawCalls;
        avg.pipelineBinds += stats.pipelineBinds;
        avg.descriptorSetBinds += stats.descriptorSetBinds;
        avg.renderPasses += stats.renderPasses;
        avg.textureMemoryBytes += stats.textureMemoryBytes;
        avg.bufferMemoryBytes += stats.bufferMemoryBytes;
        avg.textureCount += stats.textureCount;
        avg.bufferCount += stats.bufferCount;
        avg.thermalStatus = stats.thermalStatus;
        avg.throttling = stats.throttling;
        avg.frameTimeMs += stats.frameTimeMs;
        avg.fps += stats.fps;
        count++;
    }
    
    if (count > 0) {
        avg.gpuTotalMs /= count;
        avg.gpuGraphicsMs /= count;
        avg.gpuComputeMs /= count;
        avg.cpuTotalMs /= count;
        avg.cpuRecordMs /= count;
        avg.cpuSubmitMs /= count;
        avg.drawCalls /= count;
        avg.pipelineBinds /= count;
        avg.descriptorSetBinds /= count;
        avg.renderPasses /= count;
        avg.textureMemoryBytes /= count;
        avg.bufferMemoryBytes /= count;
        avg.textureCount /= count;
        avg.bufferCount /= count;
        avg.frameTimeMs /= count;
        avg.fps /= count;
        avg.framesAnalyzed = count;
    }
    
    return avg;
}

void Profiler::SetThermalCallback(ThermalCallback cb) {
    thermalCallback_ = std::move(cb);
}

void Profiler::UpdateThermalStatus() {
#if __ANDROID_API__ >= 30
    if (!thermalManager_) return;
    
    int status = AThermal_getCurrentThermalStatus(thermalManager_);
    if (status != thermalStatus_) {
        thermalStatus_ = status;
        if (thermalCallback_) {
            thermalCallback_(thermalStatus_);
        }
        LOGI("Thermal status changed: %d (%s)", status,
             status == ATHERMAL_STATUS_NONE ? "none" :
             status == ATHERMAL_STATUS_LIGHT ? "light" :
             status == ATHERMAL_STATUS_MODERATE ? "moderate" :
             status == ATHERMAL_STATUS_SEVERE ? "severe" :
             status == ATHERMAL_STATUS_CRITICAL ? "critical" :
             status == ATHERMAL_STATUS_EMERGENCY ? "emergency" :
             status == ATHERMAL_STATUS_SHUTDOWN ? "shutdown" : "unknown");
    }
#else
    // Thermal monitoring not available on API < 30
#endif
}

Profiler::PerformanceReport Profiler::GenerateReport(uint32_t frameCount) const {
    PerformanceReport report;
    auto avg = GetAverageStats(frameCount);
    report.framesAnalyzed = avg.framesAnalyzed;
    report.avgFrameTimeMs = avg.frameTimeMs;
    report.avgGpuTimeMs = avg.gpuTotalMs;
    report.avgCpuTimeMs = avg.cpuTotalMs;
    
    // Calculate P99 frame time
    std::lock_guard<std::mutex> lock(historyMutex_);
    std::vector<double> frameTimes;
    uint32_t frames = std::min(frameCount, static_cast<uint32_t>(frameHistory_.size()));
    for (uint32_t i = 0; i < frames; ++i) {
        uint32_t idx = (currentFrameIndex_ + frameHistory_.size() - 1 - i) % frameHistory_.size();
        if (frameHistory_[idx].stats.frameTimeMs > 0) {
            frameTimes.push_back(frameHistory_[idx].stats.frameTimeMs);
        }
    }
    
    if (!frameTimes.empty()) {
        std::sort(frameTimes.begin(), frameTimes.end());
        size_t p99Idx = static_cast<size_t>(frameTimes.size() * 0.99);
        report.p99FrameTimeMs = frameTimes[p99Idx];
    }
    
    // Determine bottleneck
    double gpuRatio = avg.gpuTotalMs / std::max(avg.frameTimeMs, 0.001);
    double cpuRatio = avg.cpuTotalMs / std::max(avg.frameTimeMs, 0.001);
    
    report.gpuBound = gpuRatio > 0.7;
    report.cpuBound = cpuRatio > 0.7;
    
    if (report.gpuBound && report.cpuBound) {
        report.recommendation = "Both GPU and CPU bound - reduce resolution and simplify shaders";
    } else if (report.gpuBound) {
        report.recommendation = "GPU bound - reduce resolution, MSAA, or shader complexity";
    } else if (report.cpuBound) {
        report.recommendation = "CPU bound - optimize draw call batching, reduce JNI overhead";
    } else {
        report.recommendation = "Performance within target";
    }
    
    return report;
}

std::string Profiler::ExportStatsJson() const {
    auto stats = GetLastFrameStats();
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "{"
        << "\"frameIndex\":" << stats.frameIndex << ","
        << "\"gpuTotalMs\":" << stats.gpuTotalMs << ","
        << "\"gpuGraphicsMs\":" << stats.gpuGraphicsMs << ","
        << "\"gpuComputeMs\":" << stats.gpuComputeMs << ","
        << "\"cpuTotalMs\":" << stats.cpuTotalMs << ","
        << "\"drawCalls\":" << stats.drawCalls << ","
        << "\"pipelineBinds\":" << stats.pipelineBinds << ","
        << "\"renderPasses\":" << stats.renderPasses << ","
        << "\"textureMemoryMB\":" << (stats.textureMemoryBytes / 1024.0 / 1024.0) << ","
        << "\"bufferMemoryMB\":" << (stats.bufferMemoryBytes / 1024.0 / 1024.0) << ","
        << "\"thermalStatus\":" << stats.thermalStatus << ","
        << "\"throttling\":" << (stats.throttling ? "true" : "false") << ","
        << "\"frameTimeMs\":" << stats.frameTimeMs << ","
        << "\"fps\":" << stats.fps
        << "}";
    return oss.str();
}

} // namespace vfx