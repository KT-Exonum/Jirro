package com.vfxengine.app

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.MediaStore
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
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

    // File picker for media import
    private val pickMedia = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
        if (result.resultCode == RESULT_OK && result.data != null) {
            val uri = result.data?.data
            if (uri != null) {
                nativeEngine.importMedia(uri.toString())
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        nativeEngine.create()

        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    EditorRoot(nativeEngine, ::pickMedia)
                }
            }
        }
    }

    override fun onDestroy() {
        nativeEngine.destroy()
        super.onDestroy()
    }
}
