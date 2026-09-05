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

} // extern "C"
