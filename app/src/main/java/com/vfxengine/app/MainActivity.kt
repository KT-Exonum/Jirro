package com.vfxengine.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import com.vfxengine.app.ui.EditorRoot
import com.vfxengine.app.ui.EngineSurfaceView
import com.vfxengine.app.NativeEngine

/**
 * Phase 5: Full editor UI with timeline, node editor, inspector,
 * keyframe/curve editors, media browser, and playback controls.
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
