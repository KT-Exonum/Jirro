#pragma once
// Runtime shader compilation: compile GLSL → SPIR-V on first run, cache for subsequent runs.
// Falls back to embedded/pre-compiled shaders if compilation is unavailable.

#include <string>
#include <vector>
#include <span>
#include <functional>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <unordered_map>
#include <optional>

namespace vfx {

struct ShaderCompileResult {
    bool success = false;
    std::string errorMessage;
    std::vector<uint32_t> spirv; // word array
};

using ShaderCompileCallback = std::function<void(const std::string& shaderName, ShaderCompileResult result)>;

class RuntimeShaderCompiler {
public:
    RuntimeShaderCompiler();
    ~RuntimeShaderCompiler();

    // Set the directory where compiled shaders are cached (app's private files dir).
    void SetCacheDirectory(std::string_view cacheDir);

    // Set the AAssetManager for reading GLSL source from APK assets.
    void SetAssetManager(void* assetManager); // AAssetManager* opaque

    // Trigger compilation of all known shaders. Calls onComplete when done.
    void CompileAllShaders(ShaderCompileCallback onComplete);

    // Synchronous: compile a single shader. Returns cached or freshly compiled result.
    ShaderCompileResult CompileShader(std::string_view assetPath);

    // Load SPIR-V for a shader: from cache if available, otherwise compile and cache.
    ShaderCompileResult GetOrCompileShader(std::string_view assetPath);

    // Invalidate cache (e.g. after app update or force recompile).
    void InvalidateCache();

    // Shut down background compilation thread.
    void Shutdown();

private:
    struct CompileJob {
        std::string assetPath;
        std::string glslSource;
        std::string cacheFilePath;
    };

    ShaderCompileResult CompileGLSLToSPIRV(std::string_view glslSource, std::string_view stageHint);
    ShaderCompileResult LoadFromCache(std::string_view cacheFilePath);
    bool SaveToCache(std::string_view cacheFilePath, std::span<const uint32_t> spirv);
    std::optional<std::string> LoadGLSLFromAssets(std::string_view assetPath);
    std::optional<std::string> FindGlslangValidator();

    void CompilationThreadMain();
    void ProcessJobQueue();

    std::string cacheDir_;
    void* assetManager_ = nullptr; // AAssetManager*
    std::unordered_map<std::string, std::string> assetPathToCacheKey_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<CompileJob> jobQueue_;
    std::unordered_map<std::string, ShaderCompileResult> results_;
    std::thread compileThread_;
    std::atomic<bool> stopFlag_{false};
    std::atomic<bool> shutdown_{false};
    std::atomic<bool> compiling_{false};
};

} // namespace vfx
