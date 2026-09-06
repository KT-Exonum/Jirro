package com.vfxengine.app.ui

import android.content.Context
import android.view.SurfaceHolder
import android.view.SurfaceView
import com.vfxengine.app.NativeEngine

/**
 * Section 13: "The preview should use a native rendering surface such as
 * SurfaceView where appropriate." Compose owns everything else about the
 * screen; this view exists purely to hand its [Surface] to the native
 * engine on creation and revoke it on destruction. No frame data crosses
 * this class in either direction — the native engine draws directly into
 * the surface's buffer via Vulkan/GLES.
 */
class EngineSurfaceView(context: Context, private val engine: NativeEngine) :
    SurfaceView(context), SurfaceHolder.Callback {

    init {
        holder.addCallback(this)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        engine.attachSurface(holder.surface)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        // VulkanDevice::OnSurfaceResized is driven from vkAcquireNextImageKHR
        // returning OUT_OF_DATE/SUBOPTIMAL (see VulkanDevice.cpp), so an
        // explicit resize command isn't strictly required here; left as a
        // no-op rather than adding a redundant second resize path.
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        engine.detachSurface()
    }
}
