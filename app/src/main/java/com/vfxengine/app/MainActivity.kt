package com.vfxengine.app

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.result.contract.ActivityResultContracts.StartActivityForResult
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.activity.result.ActivityResultLauncher
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
    private val pickMedia = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        uri?.let { nativeEngine.importMedia(it.toString()) }
    }

    // File picker for project files (.vfxproj)
    private val pickProject = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        uri?.let { nativeEngine.openProject(it.toString()) }
    }

    // Save As project file picker
    private val saveProjectAs = registerForActivityResult(ActivityResultContracts.CreateDocument("application/octet-stream")) { uri ->
        uri?.let { nativeEngine.saveProject(it.toString()) }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        nativeEngine.create()

        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    EditorRoot(
                        nativeEngine,
                        pickMedia = { uri -> pickMedia.launch(arrayOf("*/*")) },
                        pickProject = { uri -> pickProject.launch(arrayOf("application/octet-stream")) },
                        saveProjectAs = { uri -> saveProjectAs.launch("project.vfxproj") }
                    )
                }
            }
        }
    }

    override fun onDestroy() {
        nativeEngine.destroy()
        super.onDestroy()
    }
}
