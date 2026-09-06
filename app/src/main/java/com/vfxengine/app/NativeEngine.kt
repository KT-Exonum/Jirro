package com.vfxengine.app

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

    companion object {
        init {
            System.loadLibrary("vfxengine")
        }
    }
}
