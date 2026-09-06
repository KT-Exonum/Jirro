package com.vfxengine.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.weight
import androidx.compose.material3.Card
import androidx.compose.material3.Divider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.vfxengine.app.ui.common.EditorState
import com.vfxengine.app.ui.common.ResizablePanel
import com.vfxengine.app.ui.common.SettingsDialog
import com.vfxengine.app.ui.common.Settings
import com.vfxengine.app.ui.inspector.InspectorPanel
import com.vfxengine.app.ui.keyframe.KeyframeEditor
import com.vfxengine.app.ui.media.MediaBrowser
import com.vfxengine.app.ui.nodeeditor.NodeEditor
import com.vfxengine.app.ui.nodeeditor.NodeEditorToolbar
import com.vfxengine.app.ui.playback.PlaybackControls
import com.vfxengine.app.ui.timeline.Timeline

/**
 * Full editor layout with all panels - now with resizable panels
 */
@Composable
fun EditorRoot(engine: com.vfxengine.app.NativeEngine) {
    val state = remember { EditorState(engine) }
    val settings = remember { mutableStateOf(Settings()) }
    var showSettings by remember { mutableStateOf(false) }
    
    // Panel sizes (in dp)
    var leftWidth by remember { mutableStateOf(400f) }
    var centerWidth by remember { mutableStateOf(600f) }
    var rightWidth by remember { mutableStateOf(300f) }
    var previewHeight by remember { mutableStateOf(400f) }
    var timelineHeight by remember { mutableStateOf(200f) }
    
    Surface(
        modifier = Modifier.fillMaxSize(),
        color = Color(0xFF0F0F0F)
    ) {
        Box(modifier = Modifier.fillMaxSize()) {
            Column(modifier = Modifier.fillMaxSize()) {
                // Top toolbar
                EditorToolbar(state, settings.value, onSettingsClick = { showSettings = true })
                
                Divider(color = Color.White.copy(alpha = 0.1f))
                
                // Main content area with resizable panels
                Row(modifier = Modifier.fillMaxSize().weight(1f)) {
                    // Left panel: Node editor (resizable)
                    ResizablePanel(
                        modifier = Modifier.fillMaxHeight(),
                        initialSize = leftWidth,
                        minSize = 250f,
                        maxSize = 800f,
                        isHorizontal = true,
                        onSizeChange = { leftWidth = it }
                    ) { contentModifier ->
                        Column(modifier = contentModifier.fillMaxSize()) {
                            NodeEditorToolbar(state)
                            Divider(color = Color.White.copy(alpha = 0.1f))
                            NodeEditor(state)
                        }
                    }
                    
                    // Center: Preview + Timeline (resizable horizontally)
                    ResizablePanel(
                        modifier = Modifier.fillMaxHeight(),
                        initialSize = centerWidth,
                        minSize = 400f,
                        maxSize = 1200f,
                        isHorizontal = true,
                        onSizeChange = { centerWidth = it }
                    ) { contentModifier ->
                        Column(modifier = contentModifier.fillMaxSize()) {
                            // Preview surface (resizable vertically)
                            ResizablePanel(
                                modifier = Modifier.fillMaxWidth(),
                                initialSize = previewHeight,
                                minSize = 200f,
                                maxSize = 800f,
                                isHorizontal = false,
                                onSizeChange = { previewHeight = it }
                            ) { previewModifier ->
                                Box(
                                    modifier = previewModifier
                                        .fillMaxWidth()
                                        .background(Color.Black)
                                ) {
                                    AndroidView(
                                        factory = { context -> EngineSurfaceView(context, engine) },
                                        modifier = Modifier.fillMaxSize()
                                    )
                                }
                            }
                            
                            Divider(color = Color.White.copy(alpha = 0.1f))
                            
                            // Timeline (resizable vertically, fills remaining)
                            ResizablePanel(
                                modifier = Modifier.fillMaxWidth(),
                                initialSize = timelineHeight,
                                minSize = 120f,
                                maxSize = 500f,
                                isHorizontal = false,
                                onSizeChange = { timelineHeight = it }
                            ) { timelineModifier ->
                                Timeline(state)
                            }
                        }
                    }
                    
                    // Right panel: Inspector + Media + Keyframes (resizable)
                    ResizablePanel(
                        modifier = Modifier.fillMaxHeight(),
                        initialSize = rightWidth,
                        minSize = 250f,
                        maxSize = 600f,
                        isHorizontal = true,
                        onSizeChange = { rightWidth = it }
                    ) { contentModifier ->
                        Column(modifier = contentModifier.fillMaxSize()) {
                            TabbedRightPanel(state)
                        }
                    }
                }
                
                Divider(color = Color.White.copy(alpha = 0.1f))
                
                // Bottom: Playback controls
                PlaybackControls(state)
            }
            
            // Settings dialog overlay
            if (showSettings) {
                SettingsDialog(
                    settings = settings.value,
                    onSave = { newSettings ->
                        settings.value = newSettings
                        applySettingsToEngine(engine, newSettings)
                    },
                    onDismiss = { showSettings = false },
                    nativeEngine = engine
                )
            }
        }
    }
}

private fun applySettingsToEngine(engine: com.vfxengine.app.NativeEngine, settings: Settings) {
    // Apply settings to native engine via JNI commands
    // This would send commands to update native engine settings
    // For now, we just store them in EditorState
}

@Composable
fun EditorToolbar(
    state: EditorState,
    settings: Settings,
    onSettingsClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(48.dp)
            .padding(horizontal = 16.dp)
            .background(Color(0xFF121212)),
        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        // File menu
        Row {
            Text(text = "VFX Engine", color = Color.White, fontSize = 16.sp, fontWeight = androidx.compose.ui.text.font.FontWeight.Bold)
            androidx.compose.foundation.layout.Box(modifier = Modifier.width(16.dp))
            androidx.compose.material3.TextButton(onClick = { /* New project */ }) { Text("New", color = Color.White) }
            androidx.compose.material3.TextButton(onClick = { /* Open project */ }) { Text("Open", color = Color.White) }
            androidx.compose.material3.TextButton(onClick = { /* Save project */ }) { Text("Save", color = Color.White) }
            androidx.compose.material3.TextButton(onClick = { /* Export */ }) { Text("Export", color = Color.White) }
        }
        
        androidx.compose.foundation.layout.Box(modifier = Modifier.weight(1f))
        
        // View toggles + Settings
        Row {
            androidx.compose.material3.IconButton(onClick = { /* Toggle node editor */ }) {
                androidx.compose.material3.Icon(
                    painter = androidx.compose.ui.res.painterResource(id = android.R.drawable.ic_menu_manage),
                    contentDescription = "Nodes"
                )
            }
            androidx.compose.material3.IconButton(onClick = { /* Toggle timeline */ }) {
                androidx.compose.material3.Icon(
                    painter = androidx.compose.ui.res.painterResource(id = android.R.drawable.ic_media_play),
                    contentDescription = "Timeline"
                )
            }
            androidx.compose.material3.IconButton(onClick = { /* Toggle inspector */ }) {
                androidx.compose.material3.Icon(
                    painter = androidx.compose.ui.res.painterResource(id = android.R.drawable.ic_menu_info_details),
                    contentDescription = "Inspector"
                )
            }
            androidx.compose.material3.IconButton(onClick = onSettingsClick) {
                androidx.compose.material3.Icon(
                    painter = androidx.compose.ui.res.painterResource(id = android.R.drawable.ic_menu_preferences),
                    contentDescription = "Settings"
                )
            }
        }
    }
}

@Composable
fun TabbedRightPanel(state: EditorState) {