#include "RuntimeShaderCompiler.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <optional>
#include <sstream>
#include <chrono>
#include <unistd.h>

#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#define LOG_TAG "RuntimeShaderCompiler"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace vfx {

RuntimeShaderCompiler::RuntimeShaderCompiler() = default;

RuntimeShaderCompiler::~RuntimeShaderCompiler() {
    Shutdown();
}

void RuntimeShaderCompiler::SetCacheDirectory(std::string_view cacheDir) {
    std::lock_guard<std::mutex> lock(mutex_);
    cacheDir_ = cacheDir;
}

void RuntimeShaderCompiler::SetAssetManager(void* assetManager) {
    std::lock_guard<std::mutex> lock(mutex_);
    assetManager_ = assetManager;
}

void RuntimeShaderCompiler::CompileAllShaders(ShaderCompileCallback onComplete) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (compiling_.exchange(true)) {
        LOGW("Compilation already in progress");
        return;
    }

    // Discover shaders from a known list. In a fuller implementation this
    // would scan assets/shaders/ for *.vert/*.frag/*.comp.
    static constexpr const char* kShaderAssets[] = {
        "shaders/fullscreen.vert",
        "shaders/blend_normal.frag",
        "shaders/blend_multiply.frag",
        "shaders/blend_screen.frag",
        "shaders/blend_overlay.frag",
        "shaders/blend_add.frag",
        "shaders/blend_subtract.frag",
        "shaders/color_correction.frag",
        "shaders/blur.frag",
        "shaders/mask.frag",
        "shaders/composite.frag",
        "shaders/vector_source.vert",
        "shaders/vector_source.frag",
        "shaders/text_source.vert",
        "shaders/text_source.frag",
        "shaders/stroke_source.vert",
        "shaders/stroke_source.frag",
        "shaders/adjustment.vert",
        "shaders/adjustment.frag",
        "shaders/null_layer.vert",
        "shaders/null_layer.frag",
        "shaders/audio_reactive.frag",
        "shaders/audio_waveform.frag",
        "shaders/audio_spectrum.frag",
        "shaders/particle.frag",
        "shaders/transform3d.frag",
        "shaders/camera3d.frag",
        "shaders/mesh_pbr.frag",
        "shaders/output.frag",
        "shaders/shape2d.frag",
        "shaders/shape_merge.frag",
        "shaders/chroma_key.frag",
    };

    for (const char* assetPath : kShaderAssets) {
        CompileJob job;
        job.assetPath = assetPath;
        
        // Derive cache key from asset path
        std::replace(job.assetPath.begin(), job.assetPath.end(), '/', '_');
        std::replace(job.assetPath.begin(), job.assetPath.end(), '.', '_');
        job.cacheFilePath = cacheDir_ + "/" + job.assetPath + ".spv.cache";
        job.glslSource = ""; // loaded lazily in thread

        jobQueue_.push(std::move(job));
    }

    cv_.notify_one();
    compileThread_ = std::thread(&RuntimeShaderCompiler::CompilationThreadMain, this);
}

ShaderCompileResult RuntimeShaderCompiler::CompileShader(std::string_view assetPath) {
    ShaderCompileResult result;

    // Check cache first
    std::string cacheKey = std::string(assetPath);
    std::replace(cacheKey.begin(), cacheKey.end(), '/', '_');
    std::replace(cacheKey.begin(), cacheKey.end(), '.', '_');
    std::string cacheFilePath = cacheDir_ + "/" + cacheKey + ".spv.cache";

    if (!cacheDir_.empty()) {
        result = LoadFromCache(cacheFilePath);
        if (result.success) {
            LOGI("Loaded shader from cache: %s", std::string(assetPath).c_str());
            return result;
        }
    }

    // Load GLSL source
    auto glslOpt = LoadGLSLFromAssets(assetPath);
    if (!glslOpt) {
        result.errorMessage = "Failed to load GLSL from assets: " + std::string(assetPath);
        return result;
    }

    // Determine stage hint from extension
    std::string stageHint = "frag";
    std::string pathStr = std::string(assetPath);
    if (pathStr.ends_with(".vert")) stageHint = "vert";
    else if (pathStr.ends_with(".frag")) stageHint = "frag";
    else if (pathStr.ends_with(".comp")) stageHint = "comp";

    result = CompileGLSLToSPIRV(*glslOpt, stageHint);
    if (result.success && !cacheFilePath.empty()) {
        SaveToCache(cacheFilePath, result.spirv);
        LOGI("Cached compiled shader: %s", std::string(assetPath).c_str());
    }

    return result;
}

ShaderCompileResult RuntimeShaderCompiler::GetOrCompileShader(std::string_view assetPath) {
    return CompileShader(assetPath);
}

void RuntimeShaderCompiler::InvalidateCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    // In a real implementation, delete cache files or bump a version file.
    LOGI("Shader cache invalidated");
}

void RuntimeShaderCompiler::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_.store(true);
        cv_.notify_one();
    }
    if (compileThread_.joinable()) {
        compileThread_.join();
    }
    compiling_.store(false);
}

ShaderCompileResult RuntimeShaderCompiler::CompileGLSLToSPIRV(std::string_view glslSource, std::string_view stageHint) {
    ShaderCompileResult result;

    // Try to find glslangValidator
    auto validatorOpt = FindGlslangValidator();
    if (!validatorOpt) {
        result.errorMessage = "glslangValidator not found: " + validatorOpt.error();
        return result;
    }

    std::string validator = *validatorOpt;

    // Write GLSL to a temp file
    char tmpPath[256];
    snprintf(tmpPath, sizeof(tmpPath), "/data/local/tmp/vfx_shader_XXXXXX");
    int fd = mkstemp(tmpPath);
    if (fd < 0) {
        result.errorMessage = "Failed to create temp file for shader compilation";
        return result;
    }

    FILE* tmpFile = fdopen(fd, "w");
    if (!tmpFile) {
        close(fd);
        result.errorMessage = "Failed to open temp file";
        return result;
    }

    fwrite(glslSource.data(), 1, glslSource.size(), tmpFile);
    fclose(tmpFile);

    // Output SPIR-V to another temp file
    char outPath[256];
    snprintf(outPath, sizeof(outPath), "/data/local/tmp/vfx_shader_out_XXXXXX");
    fd = mkstemp(outPath);
    if (fd < 0) {
        result.errorMessage = "Failed to create output file for shader compilation";
        unlink(tmpPath);
        return result;
    }
    close(fd);

    // Build command: glslangValidator -V input.glsl -o output.spv
    std::string cmd = validator + " -V \"" + tmpPath + "\" -o \"" + outPath + "\"";
    LOGI("Compiling shader: %s", cmd.c_str());

    int ret = system(cmd.c_str());
    if (ret != 0) {
        result.errorMessage = "glslangValidator failed with code " + std::to_string(ret);
        unlink(tmpPath);
        unlink(outPath);
        return result;
    }

    // Read compiled SPIR-V
    std::ifstream outFile(outPath, std::ios::binary);
    if (!outFile) {
        result.errorMessage = "Failed to read compiled SPIR-V";
        unlink(tmpPath);
        unlink(outPath);
        return result;
    }

    outFile.seekg(0, std::ios::end);
    size_t size = outFile.tellg();
    outFile.seekg(0, std::ios::beg);

    if (size == 0 || size % sizeof(uint32_t) != 0) {
        result.errorMessage = "Invalid SPIR-V file size";
        unlink(tmpPath);
        unlink(outPath);
        return result;
    }

    result.spirv.resize(size / sizeof(uint32_t));
    outFile.read(reinterpret_cast<char*>(result.spirv.data()), size);
    outFile.close();

    result.success = true;

    // Cleanup temp files
    unlink(tmpPath);
    unlink(outPath);

    return result;
}

std::optional<std::string> RuntimeShaderCompiler::LoadGLSLFromAssets(std::string_view assetPath) {
    if (!assetManager_) {
        LOGE("No AAssetManager set");
        return std::nullopt;
    }

    AAssetManager* mgr = static_cast<AAssetManager*>(assetManager_);
    AAsset* asset = AAssetManager_open(mgr, std::string(assetPath).c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("Failed to open asset: %s", std::string(assetPath).c_str());
        return std::nullopt;
    }

    size_t size = AAsset_getLength(asset);
    if (size == 0) {
        AAsset_close(asset);
        return std::nullopt;
    }

    std::string source;
    source.resize(size);
    int32_t read = AAsset_read(asset, source.data(), size);
    AAsset_close(asset);

    if (read != static_cast<int32_t>(size)) {
        return std::nullopt;
    }

    return source;
}

std::optional<std::string> RuntimeShaderCompiler::FindGlslangValidator() {
    // Search in common locations
    static constexpr const char* kPaths[] = {
        "/data/local/tmp/glslangValidator",
        "/data/local/tmp/glslangValidatorArm64",
        "/data/local/tmp/glslangValidatorArm",
        "/data/local/tmp/glslangValidatorX86_64",
        "/data/local/tmp/glslangValidatorX86",
        "/data/app/glslangValidator",
        "/system/bin/glslangValidator",
        "/system/x86_64/bin/glslangValidator",
        "/system/x86/bin/glslangValidator",
        "/system/arm64/bin/glslangValidator",
        "/system/arm/bin/glslangValidator",
        "glslangValidator", // PATH
    };

    for (const char* path : kPaths) {
        if (access(path, X_OK) == 0) {
            LOGI("Found glslangValidator: %s", path);
            return std::string(path);
        }
    }

    // Try PATH
    char* pathEnv = getenv("PATH");
    if (pathEnv) {
        std::istringstream iss(pathEnv);
        std::string dir;
        while (std::getline(iss, dir, ':')) {
            std::string candidate = dir + "/glslangValidator";
            if (access(candidate.c_str(), X_OK) == 0) {
                LOGI("Found glslangValidator in PATH: %s", candidate.c_str());
                return candidate;
            }
        }
    }

    LOGE("glslangValidator not found in any known location");
    return std::nullopt;
}

ShaderCompileResult RuntimeShaderCompiler::LoadFromCache(std::string_view cacheFilePath) {
    ShaderCompileResult result;

    std::ifstream file(std::string(cacheFilePath), std::ios::binary);
    if (!file) {
        return result; // not cached
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size == 0 || size % sizeof(uint32_t) != 0) {
        return result; // invalid cache
    }

    result.spirv.resize(size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(result.spirv.data()), size);
    result.success = true;

    LOGI("Loaded shader from cache: %s", std::string(cacheFilePath).c_str());
    return result;
}

bool RuntimeShaderCompiler::SaveToCache(std::string_view cacheFilePath, std::span<const uint32_t> spirv) {
    if (cacheFilePath.empty()) return false;

    std::ofstream file(std::string(cacheFilePath), std::ios::binary);
    if (!file) {
        LOGE("Failed to open cache file for writing: %s", std::string(cacheFilePath).c_str());
        return false;
    }

    file.write(reinterpret_cast<const char*>(spirv.data()), spirv.size() * sizeof(uint32_t));
    return file.good();
}

void RuntimeShaderCompiler::CompilationThreadMain() {
    LOGI("Shader compilation thread started");

    while (!shutdown_.load()) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !jobQueue_.empty() || shutdown_.load(); });

        if (shutdown_.load()) break;

        ProcessJobQueue();
    }

    LOGI("Shader compilation thread exiting");
    compiling_.store(false);
}

void RuntimeShaderCompiler::ProcessJobQueue() {
    while (!jobQueue_.empty() && !shutdown_.load()) {
        CompileJob job = std::move(jobQueue_.front());
        jobQueue_.pop();

        // Load GLSL lazily outside the lock if needed
        if (job.glslSource.empty()) {
            auto glslOpt = LoadGLSLFromAssets(job.assetPath);
            if (!glslOpt) {
                LOGW("Failed to load shader source: %s", job.assetPath.c_str());
                continue;
            }
            job.glslSource = std::move(*glslOpt);
        }

        // Determine stage from file extension
        std::string stageHint = "frag";
        if (job.assetPath.ends_with(".vert")) stageHint = "vert";
        else if (job.assetPath.ends_with(".frag")) stageHint = "frag";
        else if (job.assetPath.ends_with(".comp")) stageHint = "comp";

        ShaderCompileResult result = CompileGLSLToSPIRV(job.glslSource, stageHint);
        if (result.success) {
            SaveToCache(job.cacheFilePath, result.spirv);
        }

        results_[job.assetPath] = result;
        LOGI("Compiled %s: %s", job.assetPath.c_str(), result.success ? "OK" : result.errorMessage.c_str());
    }
}

} // namespace vfx
