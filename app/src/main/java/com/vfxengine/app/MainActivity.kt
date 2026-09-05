package com.vfxengine.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import com.vfxengine.app.ui.EngineSurfaceView

/**
 * Phase 5 scaffold only (Section 13 lists the full editor surface — timeline,
 * node editor, inspector, keyframe/curve editors, media browser — none of
 * which are built out here). This activity's job is narrowly: own the
 * [NativeEngine]'s lifecycle correctly and host the native preview surface,
 * which is the one piece of Phase 5 that Phase 1-4 code actually depends on
 * for end-to-end validation ("does the triangle show up on screen").
 */
class MainActivity : ComponentActivity() {
    private val nativeEngine = NativeEngine()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        nativeEngine.create()

        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    EditorRoot(nativeEngine)
                }
            }
        }
    }

    override fun onDestroy() {
        nativeEngine.destroy()
        super.onDestroy()
    }
}

@Composable
fun EditorRoot(engine: NativeEngine) {
    // TODO(phase5): timeline, node editor, inspector, keyframe/curve editors,
    // media browser, playback controls all compose around this preview
    // surface. Each should talk to `engine` only through NativeEngine's
    // command methods (never a direct native handle), so the UI layer stays
    // swappable independent of the native engine's internals.
    Box(modifier = Modifier.fillMaxSize()) {
        AndroidView(
            factory = { context -> EngineSurfaceView(context, engine) },
            modifier = Modifier.fillMaxSize()
        )
    }
}
