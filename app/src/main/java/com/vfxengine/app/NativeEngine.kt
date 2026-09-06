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

    companion object {
        init {
            System.loadLibrary("vfxengine")
        }
    }
}
