// Section 12: every JNI entry point here either (a) posts a small command
// onto Engine's lock-free queue and returns immediately, or (b) is one of
// the two surface-lifecycle calls that must be synchronous by nature of
// owning an ANativeWindow. Nothing here does GPU or file I/O work on the
// calling (UI) thread.

#include <android/asset_manager.h>
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

// Dev hot-reload: store the APK's AAssetManager so VulkanDevice can read
// pre-compiled .spv shaders from assets/ at runtime.
JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeSetAssetManager(JNIEnv*, jobject, jlong handle,
                                                            jobject javaAssetManager) {
    auto* engine = GetEngine(handle);
    if (!engine) return;
    AAssetManager* mgr = AAssetManager_fromJava(env, javaAssetManager);
    engine->SetAssetManager(mgr);
}

// Runtime shader compilation: set the cache directory for compiled SPIR-V.
JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeSetShaderCacheDir(JNIEnv* env, jobject, jlong handle,
                                                              jstring cacheDir) {
    auto* engine = GetEngine(handle);
    if (!engine) return;
    const char* dirChars = env->GetStringUTFChars(cacheDir, nullptr);
    engine->SetShaderCacheDirectory(dirChars);
    env->ReleaseStringUTFChars(cacheDir, dirChars);
}

// Trigger compile-on-first-run if cache is missing/stale.
JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeCompileShadersIfNeeded(JNIEnv*, jobject, jlong handle) {
    auto* engine = GetEngine(handle);
    if (!engine) return;
    engine->QueueCommand([](vfx::Engine& eng) {
        eng.CompileShadersIfNeeded();
    });
}

// Dev hot-reload: trigger re-compilation/reload of all shaders from assets.
JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeReloadShaders(JNIEnv*, jobject, jlong handle) {
    auto* engine = GetEngine(handle);
    if (!engine) return;
    engine->QueueCommand([](vfx::Engine& eng) {
        eng.ReloadShadersFromAssets();
    });
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

// Phase 7+: Set expression on a node for procedural animation
JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeSetExpression(JNIEnv* env, jobject, jlong handle,
                                                         jstring nodeId, jstring uniformName,
                                                         jstring expression) {
    auto* engine = GetEngine(handle);
    if (!engine) return;

    const char* nodeIdChars = env->GetStringUTFChars(nodeId, nullptr);
    const char* uniformChars = env->GetStringUTFChars(uniformName, nullptr);
    const char* exprChars = env->GetStringUTFChars(expression, nullptr);

    engine->QueueCommand([nodeId = std::string(nodeIdChars), 
                          uniformName = std::string(uniformChars),
                          exprScript = std::string(exprChars)](vfx::Engine& eng) {
        if (auto* node = eng.Graph().FindNodeMutable(nodeId)) {
            vfx::Expression expr;
            expr.script = exprScript;
            node->expressions[uniformName] = std::move(expr);
        }
    });

    env->ReleaseStringUTFChars(nodeId, nodeIdChars);
    env->ReleaseStringUTFChars(uniformName, uniformChars);
    env->ReleaseStringUTFChars(expression, exprChars);
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

// Phase 7+: Node graph commands
JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeAddNode(JNIEnv* env, jobject, jlong handle,
                                                   jint kind, jfloat x, jfloat y, jstring name, jstring groupId) {
    auto* engine = GetEngine(handle);
    if (!engine) return;

    const char* nameChars = env->GetStringUTFChars(name, nullptr);
    const char* groupChars = groupId ? env->GetStringUTFChars(groupId, nullptr) : nullptr;
    
    vfx::AddNodeCommand cmd;
    cmd.nodeId = "node_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    cmd.kind = static_cast<vfx::NodeKind>(kind);
    cmd.x = x;
    cmd.y = y;
    cmd.name = nameChars;
    cmd.groupId = groupChars ? groupChars : "";

    env->ReleaseStringUTFChars(name, nameChars);
    if (groupChars) env->ReleaseStringUTFChars(groupId, groupChars);

    engine->QueueAddNode(std::move(cmd));
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeRemoveNode(JNIEnv* env, jobject, jlong handle,
                                                      jstring nodeId) {
    auto* engine = GetEngine(handle);
    if (!engine) return;

    const char* nodeIdChars = env->GetStringUTFChars(nodeId, nullptr);
    vfx::RemoveNodeCommand cmd{nodeIdChars};
    env->ReleaseStringUTFChars(nodeId, nodeIdChars);

    engine->QueueRemoveNode(std::move(cmd));
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeConnectNodes(JNIEnv* env, jobject, jlong handle,
                                                        jstring fromNodeId, jstring fromSlot,
                                                        jstring toNodeId, jstring toSlot) {
    auto* engine = GetEngine(handle);
    if (!engine) return;

    const char* fromNodeChars = env->GetStringUTFChars(fromNodeId, nullptr);
    const char* fromSlotChars = env->GetStringUTFChars(fromSlot, nullptr);
    const char* toNodeChars = env->GetStringUTFChars(toNodeId, nullptr);
    const char* toSlotChars = env->GetStringUTFChars(toSlot, nullptr);

    vfx::ConnectNodesCommand cmd{fromNodeChars, fromSlotChars, toNodeChars, toSlotChars};

    env->ReleaseStringUTFChars(fromNodeId, fromNodeChars);
    env->ReleaseStringUTFChars(fromSlot, fromSlotChars);
    env->ReleaseStringUTFChars(toNodeId, toNodeChars);
    env->ReleaseStringUTFChars(toSlot, toSlotChars);

    engine->QueueConnectNodes(std::move(cmd));
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeSetNodeParent(JNIEnv* env, jobject, jlong handle,
                                                         jstring nodeId, jstring parentNodeId) {
    auto* engine = GetEngine(handle);
    if (!engine) return;

    const char* nodeIdChars = env->GetStringUTFChars(nodeId, nullptr);
    const char* parentChars = parentNodeId ? env->GetStringUTFChars(parentNodeId, nullptr) : nullptr;

    vfx::SetNodeParentCommand cmd{nodeIdChars, parentChars ? parentChars : ""};

    env->ReleaseStringUTFChars(nodeId, nodeIdChars);
    if (parentChars) env->ReleaseStringUTFChars(parentNodeId, parentChars);

    engine->QueueSetNodeParent(std::move(cmd));
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeCreateGroup(JNIEnv* env, jobject, jlong handle,
                                                       jstring groupId, jstring name, jobjectArray memberIds) {
    auto* engine = GetEngine(handle);
    if (!engine) return;

    const char* groupIdChars = env->GetStringUTFChars(groupId, nullptr);
    const char* nameChars = env->GetStringUTFChars(name, nullptr);

    vfx::CreateGroupCommand cmd;
    cmd.groupId = groupIdChars;
    cmd.name = nameChars;

    jsize count = env->GetArrayLength(memberIds);
    for (jsize i = 0; i < count; ++i) {
        jstring memberId = (jstring)env->GetObjectArrayElement(memberIds, i);
        const char* memberChars = env->GetStringUTFChars(memberId, nullptr);
        cmd.memberNodeIds.push_back(memberChars);
        env->ReleaseStringUTFChars(memberId, memberChars);
    }

    env->ReleaseStringUTFChars(groupId, groupIdChars);
    env->ReleaseStringUTFChars(name, nameChars);

    engine->QueueCreateGroup(std::move(cmd));
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeRemoveGroup(JNIEnv* env, jobject, jlong handle,
                                                       jstring groupId) {
    auto* engine = GetEngine(handle);
    if (!engine) return;

    const char* groupIdChars = env->GetStringUTFChars(groupId, nullptr);
    vfx::RemoveGroupCommand cmd{groupIdChars};
    env->ReleaseStringUTFChars(groupId, groupIdChars);

    engine->QueueRemoveGroup(std::move(cmd));
}

// Timeline clip commands
JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeAddClip(JNIEnv* env, jobject, jlong handle,
                                                   jstring clipId, jstring sourceNodeId,
                                                   jint type, jdouble timelineStart,
                                                   jdouble sourceIn, jdouble sourceOut,
                                                   jdouble speed, jint layer) {
    auto* engine = GetEngine(handle);
    if (!engine) return;

    const char* clipIdChars = env->GetStringUTFChars(clipId, nullptr);
    const char* sourceNodeChars = env->GetStringUTFChars(sourceNodeId, nullptr);

    vfx::AddClipCommand cmd;
    cmd.clipId = clipIdChars;
    cmd.sourceNodeId = sourceNodeChars;
    cmd.type = static_cast<vfx::Timeline::ClipType>(type);
    cmd.timelineStart = timelineStart;
    cmd.sourceInPoint = sourceIn;
    cmd.sourceOutPoint = sourceOut;
    cmd.playbackSpeed = speed;
    cmd.layer = layer;

    env->ReleaseStringUTFChars(clipId, clipIdChars);
    env->ReleaseStringUTFChars(sourceNodeId, sourceNodeChars);

    engine->QueueAddClip(std::move(cmd));
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeRemoveClip(JNIEnv* env, jobject, jlong handle,
                                                      jstring clipId) {
    auto* engine = GetEngine(handle);
    if (!engine) return;

    const char* clipIdChars = env->GetStringUTFChars(clipId, nullptr);
    vfx::RemoveClipCommand cmd{clipIdChars};
    env->ReleaseStringUTFChars(clipId, clipIdChars);

    engine->QueueRemoveClip(std::move(cmd));
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeUpdateClip(JNIEnv* env, jobject, jlong handle,
                                                      jstring clipId, jdouble timelineStart,
                                                      jdouble sourceIn, jdouble sourceOut,
                                                      jdouble speed, jint layer,
                                                      jboolean enabled, jboolean locked,
                                                      jboolean hasTimelineStart, jboolean hasSourceIn,
                                                      jboolean hasSourceOut, jboolean hasSpeed,
                                                      jboolean hasLayer, jboolean hasEnabled,
                                                      jboolean hasLocked) {
    auto* engine = GetEngine(handle);
    if (!engine) return;

    const char* clipIdChars = env->GetStringUTFChars(clipId, nullptr);

    vfx::UpdateClipCommand cmd;
    cmd.clipId = clipIdChars;
    if (hasTimelineStart) cmd.timelineStart = timelineStart;
    if (hasSourceIn) cmd.sourceInPoint = sourceIn;
    if (hasSourceOut) cmd.sourceOutPoint = sourceOut;
    if (hasSpeed) cmd.playbackSpeed = speed;
    if (hasLayer) cmd.layer = layer;
    if (hasEnabled) cmd.enabled = enabled;
    if (hasLocked) cmd.locked = locked;

    env->ReleaseStringUTFChars(clipId, clipIdChars);

    engine->QueueUpdateClip(std::move(cmd));
}

// Undo/Redo
JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeUndo(JNIEnv*, jobject, jlong handle) {
    auto* engine = GetEngine(handle);
    if (engine) engine->QueueUndo({});
}

JNIEXPORT void JNICALL
Java_com_vfxengine_app_NativeEngine_nativeRedo(JNIEnv*, jobject, jlong handle) {
    auto* engine = GetEngine(handle);
    if (engine) engine->QueueRedo({});
}

} // extern "C"
