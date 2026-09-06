// Section 12: every JNI entry point here either (a) posts a small command
// onto Engine's lock-free queue and returns immediately, or (b) is one of
// the two surface-lifecycle calls that must be synchronous by nature of
// owning an ANativeWindow. Nothing here does GPU or file I/O work on the
// calling (UI) thread.

#include <android/native_window_jni.h>
#include <jni.h>

#include <memory>

#include "Engine.h"

namespace {
// One Engine instance per process; NativeEngine.kt holds the returned
// pointer as a Long and passes it back into every subsequent call. A
// multi-project-instance app would key this by project id instead.
vfx::Engine* GetEngine(jlong handle) { return reinterpret_cast<vfx::Engine*>(handle); }
} // namespace

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_vfxengine_app_NativeEngine_nativeCreate(JNIEnv*, jobject) {
    auto* engine = new vfx::Engine();
    engine->Start();
    return reinterpret_cast<jlong>(engine);
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeDestroy(JNIEnv*, jobject, jlong handle) {
    auto* engine = GetEngine(handle);
    if (!engine) return;
    engine->Stop();
    delete engine;
}

// Synchronous by necessity — see file header comment.
JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeAttachSurface(JNIEnv* env, jobject, jlong handle,
                                                         jobject surface) {
    auto* engine = GetEngine(handle);
    if (!engine) return;
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    engine->AttachSurface(window);
    if (window) ANativeWindow_release(window); // Engine::AttachSurface acquired its own ref
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeDetachSurface(JNIEnv*, jobject, jlong handle) {
    auto* engine = GetEngine(handle);
    if (engine) engine->DetachSurface();
}

// Fire-and-forget command, per Section 12's UpdateUniform example.
JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeUpdateUniform(JNIEnv* env, jobject, jlong handle,
                                                         jstring nodeId, jstring uniformName,
                                                         jfloat value) {
    auto* engine = GetEngine(handle);
    if (!engine) return;

    const char* nodeIdChars = env->GetStringUTFChars(nodeId, nullptr);
    const char* uniformChars = env->GetStringUTFChars(uniformName, nullptr);
    vfx::UpdateUniformCommand cmd{nodeIdChars, uniformChars, value};
    env->ReleaseStringUTFChars(nodeId, nodeIdChars);
    env->ReleaseStringUTFChars(uniformName, uniformChars);

    engine->QueueUniformUpdate(std::move(cmd));
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeSeek(JNIEnv*, jobject, jlong handle, jdouble seconds) {
    auto* engine = GetEngine(handle);
    if (engine) engine->QueueSeek(seconds);
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeSetPlaying(JNIEnv*, jobject, jlong handle,
                                                      jboolean playing) {
    auto* engine = GetEngine(handle);
    if (engine) engine->QueueSetPlaying(playing == JNI_TRUE);
}

// Phase 4: Timeline control
JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeSetPlaybackSpeed(JNIEnv*, jobject, jlong handle,
                                                            jdouble speed) {
    auto* engine = GetEngine(handle);
    if (engine) engine->QueueSetPlaybackSpeed(speed);
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeSetScrubbing(JNIEnv*, jobject, jlong handle,
                                                        jboolean scrubbing) {
    auto* engine = GetEngine(handle);
    if (engine) engine->QueueSetScrubbing(scrubbing == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeSetMasterSpeed(JNIEnv*, jobject, jlong handle,
                                                          jdouble speed) {
    auto* engine = GetEngine(handle);
    if (engine) engine->QueueSetMasterSpeed(speed);
}

// Phase 6: Export
JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeExport(JNIEnv* env, jobject, jlong handle,
                                                  jstring outputPath, jint width, jint height,
                                                  jdouble frameRate, jdouble startTime, jdouble endTime,
                                                  jint bitrateMbps, jstring codec) {
    auto* engine = GetEngine(handle);
    if (!engine) return;

    const char* outputPathChars = env->GetStringUTFChars(outputPath, nullptr);
    const char* codecChars = env->GetStringUTFChars(codec, nullptr);

    vfx::ExportCommand cmd;
    cmd.outputPath = outputPathChars;
    cmd.width = static_cast<uint32_t>(width);
    cmd.height = static_cast<uint32_t>(height);
    cmd.frameRate = frameRate;
    cmd.startTime = startTime;
    cmd.endTime = endTime;
    cmd.bitrateMbps = bitrateMbps;
    cmd.codec = codecChars;

    // Completion callback - posts result back to UI via JNI
    cmd.onComplete = [env, weakEngine = jlong(handle)](vfx::ExportResult result) {
        // In real implementation, would call a Java callback method
        // For now, just log
        __android_log_print(ANDROID_LOG_INFO, "ExportPipeline",
            "Export complete: success=%d, frames=%llu, time=%.2fs, fps=%.1f",
            result.success, result.framesEncoded, result.elapsedSeconds, result.averageFps);
    };

    env->ReleaseStringUTFChars(outputPath, outputPathChars);
    env->ReleaseStringUTFChars(codec, codecChars);

    engine->QueueExport(std::move(cmd));
}

// Phase 6: Project save/load
JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeSaveProject(JNIEnv* env, jobject, jlong handle,
                                                       jstring filePath) {
    auto* engine = GetEngine(handle);
    if (!engine) return;

    const char* pathChars = env->GetStringUTFChars(filePath, nullptr);
    vfx::SaveProjectCommand cmd{pathChars};
    cmd.onComplete = [](bool success) {
        __android_log_print(ANDROID_LOG_INFO, "ProjectManager", "Save project: %s", success ? "success" : "failed");
    };
    env->ReleaseStringUTFChars(filePath, pathChars);

    engine->QueueSaveProject(std::move(cmd));
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeLoadProject(JNIEnv* env, jobject, jlong handle,
                                                       jstring filePath) {
    auto* engine = GetEngine(handle);
    if (!engine) return;

    const char* pathChars = env->GetStringUTFChars(filePath, nullptr);
    vfx::LoadProjectCommand cmd{pathChars};
    cmd.onComplete = [](bool success) {
        __android_log_print(ANDROID_LOG_INFO, "ProjectManager", "Load project: %s", success ? "success" : "failed");
    };
    env->ReleaseStringUTFChars(filePath, pathChars);

    engine->QueueLoadProject(std::move(cmd));
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeNewProject(JNIEnv*, jobject, jlong handle,
                                                      jstring name) {
    auto* engine = GetEngine(handle);
    if (!engine) return;
    
    // Get project manager and create new project
    // For now, just log
    __android_log_print(ANDROID_LOG_INFO, "ProjectManager", "New project requested");
}

// Phase 6: Profiling
JNIEXPORT jstring JNICALL
Java_com_vfxengine_app_NativeEngine_nativeGetProfileStats(JNIEnv* env, jobject, jlong handle) {
    auto* engine = GetEngine(handle);
    if (!engine) return env->NewStringUTF("{}");
    
    auto* profiler = engine->GetProfiler();
    if (!profiler) return env->NewStringUTF("{}");
    
    std::string json = profiler->ExportStatsJson();
    return env->NewStringUTF(json.c_str());
}

} // extern "C"
