package com.vfxengine.app

import android.content.res.AssetManager
import android.view.Surface

/**
 * Thin Kotlin-side wrapper over the JNI bridge (Section 12). Every method
 * below either posts a command onto the native engine's lock-free queue and
 * returns immediately, or — for [attachSurface]/[detachSurface] — performs
 * the one synchronous handshake required to hand over an [ANativeWindow]
 * whose lifetime the Android framework controls.
 *
 * The UI (Compose) never touches native rendering state directly; it only
 * ever calls through here, so "UI thread never blocks on rendering"
 * (Section 12/14/18) is enforced at a single, auditable boundary.
 */
class NativeEngine {
    private var handle: Long = 0L

    fun create() {
        check(handle == 0L) { "NativeEngine already created" }
        handle = nativeCreate()
    }

    fun destroy() {
        if (handle != 0L) {
            nativeDestroy(handle)
            handle = 0L
        }
    }

    fun attachSurface(surface: Surface) {
        if (handle != 0L) nativeAttachSurface(handle, surface)
    }

    fun detachSurface() {
        if (handle != 0L) nativeDetachSurface(handle)
    }

    fun updateUniform(nodeId: String, uniformName: String, value: Float) {
        if (handle != 0L) nativeUpdateUniform(handle, nodeId, uniformName, value)
    }

    fun seek(seconds: Double) {
        if (handle != 0L) nativeSeek(handle, seconds)
    }

    fun setPlaying(playing: Boolean) {
        if (handle != 0L) nativeSetPlaying(handle, playing)
    }

    // Phase 4: Timeline control
    fun setPlaybackSpeed(speed: Double) {
        if (handle != 0L) nativeSetPlaybackSpeed(handle, speed)
    }

    fun setScrubbing(scrubbing: Boolean) {
        if (handle != 0L) nativeSetScrubbing(handle, scrubbing)
    }

    fun setMasterSpeed(speed: Double) {
        if (handle != 0L) nativeSetMasterSpeed(handle, speed)
    }

    // Phase 6: Export
    fun export(
        outputPath: String,
        width: Int = 1920,
        height: Int = 1080,
        frameRate: Double = 30.0,
        startTime: Double = 0.0,
        endTime: Double = 10.0,
        bitrateMbps: Int = 20,
        codec: String = "video/avc"
    ) {
        if (handle != 0L) nativeExport(handle, outputPath, width, height, frameRate, startTime, endTime, bitrateMbps, codec)
    }

    // Phase 6: Project management
    fun saveProject(filePath: String?) {
        if (handle != 0L) nativeSaveProject(handle, filePath ?: "")
    }

    fun loadProject(filePath: String) {
        if (handle != 0L) nativeLoadProject(handle, filePath)
    }

    fun newProject(name: String = "Untitled Project") {
        if (handle != 0L) nativeNewProject(handle, name)
    }

    // Phase 6: Profiling
    fun getProfileStats(): String {
        if (handle != 0L) return nativeGetProfileStats(handle)
        return "{}"
    }

    // Phase 6: Dev shader hot-reload
    fun setAssetManager(assetManager: AssetManager) {
        if (handle != 0L) nativeSetAssetManager(handle, assetManager)
    }

    fun reloadShaders() {
        if (handle != 0L) nativeReloadShaders(handle)
    }

    // Phase 7+: Node graph commands
    fun addNode(kind: Int, x: Float, y: Float, name: String, groupId: String = "") {
        if (handle != 0L) nativeAddNode(handle, kind, x, y, name, groupId)
    }

    fun removeNode(nodeId: String) {
        if (handle != 0L) nativeRemoveNode(handle, nodeId)
    }

    fun connectNodes(fromNodeId: String, fromSlot: String, toNodeId: String, toSlot: String) {
        if (handle != 0L) nativeConnectNodes(handle, fromNodeId, fromSlot, toNodeId, toSlot)
    }

    fun setNodeParent(nodeId: String, parentNodeId: String) {
        if (handle != 0L) nativeSetNodeParent(handle, nodeId, parentNodeId)
    }

    fun createGroup(groupId: String, name: String, memberIds: Array<String>) {
        if (handle != 0L) nativeCreateGroup(handle, groupId, name, memberIds)
    }

    fun removeGroup(groupId: String) {
        if (handle != 0L) nativeRemoveGroup(handle, groupId)
    }

    // Timeline clip commands
    fun addClip(clipId: String, sourceNodeId: String, type: Int, timelineStart: Double, sourceIn: Double, sourceOut: Double, speed: Double, layer: Int) {
        if (handle != 0L) nativeAddClip(handle, clipId, sourceNodeId, type, timelineStart, sourceIn, sourceOut, speed, layer)
    }

    fun removeClip(clipId: String) {
        if (handle != 0L) nativeRemoveClip(handle, clipId)
    }

    fun updateClip(
        clipId: String,
        timelineStart: Double? = null,
        sourceIn: Double? = null,
        sourceOut: Double? = null,
        speed: Double? = null,
        layer: Int? = null,
        enabled: Boolean? = null,
        locked: Boolean? = null
    ) {
        if (handle != 0L) {
            nativeUpdateClip(
                handle, clipId,
                timelineStart ?: 0.0, sourceIn ?: 0.0, sourceOut ?: 0.0, speed ?: 0.0, layer ?: 0,
                enabled ?: false, locked ?: false,
                timelineStart != null, sourceIn != null, sourceOut != null, speed != null, layer != null, enabled != null, locked != null
            )
        }
    }

    // Expression engine
    fun setExpression(nodeId: String, uniformName: String, script: String) {
        if (handle != 0L) nativeSetExpression(handle, nodeId, uniformName, script)
    }

    // Undo/Redo
    fun undo() {
        if (handle != 0L) nativeUndo(handle)
    }

    fun redo() {
        if (handle != 0L) nativeRedo(handle)
    }

    // Audio output control
    fun startAudioOutput() {
        if (handle != 0L) nativeStartAudioOutput(handle)
    }

    fun stopAudioOutput() {
        if (handle != 0L) nativeStopAudioOutput(handle)
    }

    // Timeline clip operations
    fun splitClip(clipId: String, timelinePosition: Double) {
        if (handle != 0L) nativeSplitClip(handle, clipId, timelinePosition)
    }

    fun trimClip(clipId: String, sourceIn: Double, sourceOut: Double) {
        if (handle != 0L) nativeTrimClip(handle, clipId, sourceIn, sourceOut)
    }

    fun createTransition(fromClipId: String, toClipId: String, duration: Double, blendShaderNodeId: String) {
        if (handle != 0L) nativeCreateTransition(handle, fromClipId, toClipId, duration, blendShaderNodeId)
    }

    private external fun nativeCreate(): Long
    private external fun nativeDestroy(handle: Long)
    private external fun nativeAttachSurface(handle: Long, surface: Surface)
    private external fun nativeDetachSurface(handle: Long)
    private external fun nativeUpdateUniform(handle: Long, nodeId: String, uniformName: String, value: Float)
    private external fun nativeSeek(handle: Long, seconds: Double)
    private external fun nativeSetPlaying(handle: Long, playing: Boolean)
    private external fun nativeSetPlaybackSpeed(handle: Long, speed: Double)
    private external fun nativeSetScrubbing(handle: Long, scrubbing: Boolean)
    private external fun nativeSetMasterSpeed(handle: Long, speed: Double)
    private external fun nativeExport(handle: Long, outputPath: String, width: Int, height: Int, frameRate: Double, startTime: Double, endTime: Double, bitrateMbps: Int, codec: String)
    private external fun nativeSaveProject(handle: Long, filePath: String)
    private external fun nativeLoadProject(handle: Long, filePath: String)
    private external fun nativeNewProject(handle: Long, name: String)
    private external fun nativeGetProfileStats(handle: Long): String

    // Phase 6: Dev shader hot-reload
    private external fun nativeSetAssetManager(handle: Long, assetManager: AssetManager)
    private external fun nativeReloadShaders(handle: Long)

    // Phase 7+ externals
    private external fun nativeAddNode(handle: Long, kind: Int, x: Float, y: Float, name: String, groupId: String)
    private external fun nativeRemoveNode(handle: Long, nodeId: String)
    private external fun nativeConnectNodes(handle: Long, fromNodeId: String, fromSlot: String, toNodeId: String, toSlot: String)
    private external fun nativeSetNodeParent(handle: Long, nodeId: String, parentNodeId: String)
    private external fun nativeCreateGroup(handle: Long, groupId: String, name: String, memberIds: Array<String>)
    private external fun nativeRemoveGroup(handle: Long, groupId: String)
    private external fun nativeAddClip(handle: Long, clipId: String, sourceNodeId: String, type: Int, timelineStart: Double, sourceIn: Double, sourceOut: Double, speed: Double, layer: Int)
    private external fun nativeRemoveClip(handle: Long, clipId: String)
    private external fun nativeUpdateClip(
        handle: Long,
        clipId: String,
        timelineStart: Double, sourceIn: Double, sourceOut: Double, speed: Double, layer: Int,
        enabled: Boolean, locked: Boolean,
        hasTimelineStart: Boolean, hasSourceIn: Boolean, hasSourceOut: Boolean, hasSpeed: Boolean, hasLayer: Boolean, hasEnabled: Boolean, hasLocked: Boolean
    )
    private external fun nativeUndo(handle: Long)
    private external fun nativeRedo(handle: Long)
    private external fun nativeSetExpression(handle: Long, nodeId: String, uniformName: String, script: String)

    // Audio output control
    private external fun nativeStartAudioOutput(handle: Long)
    private external fun nativeStopAudioOutput(handle: Long)

    // Timeline clip operations
    private external fun nativeSplitClip(handle: Long, clipId: String, timelinePosition: Double)
    private external fun nativeTrimClip(handle: Long, clipId: String, sourceIn: Double, sourceOut: Double)
    private external fun nativeCreateTransition(handle: Long, fromClipId: String, toClipId: String, duration: Double, blendShaderNodeId: String)

    companion object {
        init {
            System.loadLibrary("vfxengine")
        }
    }
}
