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
import com.vfxengine.app.ui.inspector.InspectorPanel
import com.vfxengine.app.ui.keyframe.KeyframeEditor
import com.vfxengine.app.ui.media.MediaBrowser
import com.vfxengine.app.ui.nodeeditor.NodeEditor
import com.vfxengine.app.ui.nodeeditor.NodeEditorToolbar
import com.vfxengine.app.ui.playback.PlaybackControls
import com.vfxengine.app.ui.timeline.Timeline

/**
 * Full editor layout with all panels
 */
@Composable
fun EditorRoot(engine: com.vfxengine.app.NativeEngine) {
    val state = remember { EditorState(engine) }
    
    Surface(
        modifier = Modifier.fillMaxSize(),
        color = Color(0xFF0F0F0F)
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Top toolbar
            EditorToolbar(state)
            
            Divider(color = Color.White.copy(alpha = 0.1f))
            
            // Main content area
            Row(modifier = Modifier.fillMaxSize().weight(1f)) {
                // Left panel: Node editor
                Column(modifier = Modifier.width(0.4f).fillMaxHeight()) {
                    NodeEditorToolbar(state)
                    Divider(color = Color.White.copy(alpha = 0.1f))
                    NodeEditor(state)
                }
                
                Divider(color = Color.White.copy(alpha = 0.1f))
                
                // Center: Preview + Timeline
                Column(modifier = Modifier.width(0.4f).fillMaxHeight().weight(1f)) {
                    // Preview surface
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .weight(1f)
                            .background(Color.Black)
                    ) {
                        AndroidView(
                            factory = { context -> EngineSurfaceView(context, engine) },
                            modifier = Modifier.fillMaxSize()
                        )
                    }
                    
                    Divider(color = Color.White.copy(alpha = 0.1f))
                    
                    // Timeline
                    Timeline(state)
                }
                
                Divider(color = Color.White.copy(alpha = 0.1f))
                
                // Right panel: Inspector + Media + Keyframes
                Column(modifier = Modifier.width(0.2f).fillMaxHeight()) {
                    // Tabbed panel
                    TabbedRightPanel(state)
                }
            }
            
            Divider(color = Color.White.copy(alpha = 0.1f))
            
            // Bottom: Playback controls
            PlaybackControls(state)
        }
    }
}

@Composable
fun EditorToolbar(state: EditorState) {
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
        
        // View toggles
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
        }
    }
}

@Composable
fun TabbedRightPanel(state: EditorState) {
    var selectedTab by remember { mutableStateOf(0) }
    
    Column(modifier = Modifier.fillMaxSize()) {
        // Tab bar
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(40.dp)
                .background(Color(0xFF1E1E1E))
        ) {
            listOf("Inspector", "Media", "Keyframes").forEachIndexed { index, title ->
                androidx.compose.material3.TextButton(
                    onClick = { selectedTab = index },
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(40.dp)
                ) {
                    Text(
                        text = title,
                        color = if (selectedTab == index) Color.Cyan else Color.White.copy(alpha = 0.7f),
                        fontSize = 12.sp,
                        fontWeight = if (selectedTab == index) androidx.compose.ui.text.font.FontWeight.Bold else androidx.compose.ui.text.font.FontWeight.Normal
                    )
                }
            }
        }
        
        Divider(color = Color.White.copy(alpha = 0.1f))
        
        // Tab content
        when (selectedTab) {
            0 -> InspectorPanel(state)
            1 -> MediaBrowser(state)
            2 -> KeyframeEditor(state)
        }
    }
}