package com.vfxengine.app.ui.tab

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Card
import androidx.compose.material3.Divider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.vfxengine.app.ui.common.EditorState

/**
 * Edit Tab - DaVinci Resolve style Edit page
 * Timeline-centric with source/record viewers, audio meters, trim tools
 */
@Composable
fun EditTab(state: EditorState) {
    Column(modifier = Modifier.fillMaxSize()) {
        // Top toolbar with edit modes
        EditToolbar(state)
        
        Divider(color = Color.White.copy(alpha = 0.1f))
        
        // Main content area
        Row(modifier = Modifier.fillMaxSize().weight(1f)) {
            // Left: Source viewer + Media pool (compact)
            SourceViewerPanel(state)
            
            Divider(color = Color.White.copy(alpha = 0.1f))
            
            // Center: Timeline (main focus)
            TimelinePanel(state)
            
            Divider(color = Color.White.copy(alpha = 0.1f))
            
            // Right: Record viewer + Inspector + Audio meters
            RecordViewerPanel(state)
        }
        
        Divider(color = Color.White.copy(alpha = 0.1f))
        
        // Bottom: Playback controls + Audio meters
        PlaybackControlsPanel(state)
    }
}

@Composable
fun EditToolbar(state: EditorState) {
    var editMode by remember { mutableStateOf(EditMode.Select) }
    
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(48.dp)
            .padding(horizontal = 16.dp)
            .background(Color(0xFF121212)),
        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Edit mode buttons
        Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(4.dp)) {
            EditMode.values().forEach { mode ->
                androidx.compose.material3.IconButton(
                    onClick = { editMode = mode },
                    colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                        containerColor = if (editMode == mode) Color.Cyan.copy(alpha = 0.2f) else Color.Transparent
                    )
                ) {
                    Icon(
                        painter = painterResource(id = mode.iconRes),
                        contentDescription = mode.label,
                        tint = if (editMode == mode) Color.Cyan else Color.White
                    )
                }
            }
        }
        
        androidx.compose.foundation.layout.Box(modifier = Modifier.weight(1f))
        
        // Timeline zoom
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(painter = painterResource(id = android.R.drawable.ic_media_rew), contentDescription = "Zoom out", tint = Color.White.copy(alpha = 0.7f))
            Slider(
                modifier = Modifier.width(150.dp),
                value = state.timeScale / 200f,
                onValueChange = { state.timeScale = it * 200f },
                colors = androidx.compose.material3.SliderDefaults.colors(thumbColor = Color.Cyan, activeTrackColor = Color.Cyan)
            )
            Icon(painter = painterResource(id = android.R.drawable.ic_media_ff), contentDescription = "Zoom in", tint = Color.White.copy(alpha = 0.7f))
        }
        
        androidx.compose.foundation.layout.Box(modifier = Modifier.width(16.dp))
        
        // Snap toggle
        androidx.compose.material3.IconButton(
            onClick = { /* toggle snap */ },
            colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                containerColor = Color.Cyan.copy(alpha = 0.2f)
            )
        ) {
            Icon(painter = painterResource(id = android.R.drawable.ic_media_next), contentDescription = "Snap", tint = Color.Cyan)
        }
    }
}

enum class EditMode(val label: String, val iconRes: Int) {
    Select("Select", android.R.drawable.ic_menu_selectall),
    Blade("Blade", android.R.drawable.ic_menu_crop),
    Trim("Trim", android.R.drawable.ic_media_pause),
    Range("Range", android.R.drawable.ic_media_ff),
    Hand("Hand", android.R.drawable.ic_media_play)
}

@Composable
fun SourceViewerPanel(state: EditorState) {
    Card(
        modifier = Modifier
            .width(320.dp)
            .fillMaxHeight()
            .background(Color(0xFF121212))
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Source viewer
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(16f / 9f)
                    .background(Color.Black)
            ) {
                // Would show source clip preview here
                CenteredText("Source Viewer")
            }
            
            Divider(color = Color.White.copy(alpha = 0.1f))
            
            // Compact media pool for dragging to timeline
            Text(text = "Media Pool", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp, fontWeight = FontWeight.Bold, modifier = Modifier.padding(16.dp))
            
            androidx.compose.foundation.lazy.LazyColumn(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(8.dp),
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
            ) {
                items(state.mediaFiles) { media ->
                    MediaPoolItem(media = media, onDragToTimeline = { /* handle drag */ })
                }
            }
        }
    }
}

@Composable
fun MediaPoolItem(
    media: EditorState.MediaFile,
    onDragToTimeline: () -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .background(Color(0xFF1E1E1E))
            .pointerInput(media) {
                detectDragGestures(
                    onDragStart = { },
                    onDrag = { change, dragAmount ->
                        onDragToTimeline()
                    },
                    onDragEnd = { }
                )
            }
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(12.dp)
                .height(64.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(
                modifier = Modifier
                    .width(112.dp)
                    .height(64.dp)
                    .background(Color(0xFF121212))
            )
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.width(8.dp))
            
            Column(modifier = Modifier.weight(1f), verticalArrangement = androidx.compose.foundation.layout.Arrangement.Center) {
                Text(text = media.name, color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Medium, maxLines = 1, overflow = androidx.compose.ui.text.TextOverflow.Ellipsis)
                Text(text = "${media.width}×${media.height} · ${formatDuration(media.duration)}", color = Color.White.copy(alpha = 0.6f), fontSize = 10.sp)
            }
        }
    }
}

@Composable
fun TimelinePanel(state: EditorState) {
    // Full-width timeline with tracks
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .fillMaxHeight()
            .background(Color(0xFF0F0F0F))
            .pointerInput(state) {
                detectDragGestures(
                    onDrag = { change, dragAmount ->
                        state.timeOffset += dragAmount.x
                    }
                )
            }
    ) {
        // Timeline ruler
        Column(modifier = Modifier.fillMaxWidth().padding(top = 8.dp)) {
            TimelineRuler(state)
        }
        
        // Video tracks
        Column(modifier = Modifier.fillMaxWidth().padding(top = 40.dp)) {
            repeat(4) { trackIndex ->
                VideoTrackView(
                    state = state,
                    trackIndex = trackIndex,
                    trackHeight = 80f
                )
            }
        }
        
        // Audio tracks
        Column(modifier = Modifier.fillMaxWidth().padding(top = 8.dp)) {
            repeat(3) { trackIndex ->
                AudioTrackView(
                    state = state,
                    trackIndex = trackIndex,
                    trackHeight = 60f
                )
            }
        }
        
        // Playhead
        PlayheadOverlay(state, trackHeight = 80f, layerCount = 7)
    }
}

@Composable
fun VideoTrackView(
    state: EditorState,
    trackIndex: Int,
    trackHeight: Float
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(trackHeight.dp)
            .background(if (trackIndex % 2 == 0) Color(0xFF1A1A1A) else Color(0xFF121212))
    ) {
        Box(modifier = Modifier.fillMaxSize().padding(4.dp)) {
            // Track header
            Box(
                modifier = Modifier
                    .width(80.dp)
                    .fillMaxHeight()
                    .background(Color(0xFF0D0D0D))
                    .padding(8.dp)
            ) {
                Text(text = "V${trackIndex + 1}", color = Color.White.copy(alpha = 0.5f), fontSize = 11.sp, fontWeight = FontWeight.Bold)
            }
            
            // Clips on this track
            Box(modifier = Modifier
                .fillMaxWidth()
                .offset(x = 80.dp)
                .fillMaxHeight()
                .padding(4.dp)
            ) {
                // Clips would be rendered here based on timeline position
                CenteredText("Video Track ${trackIndex + 1}")
            }
        }
    }
}

@Composable
fun AudioTrackView(
    state: EditorState,
    trackIndex: Int,
    trackHeight: Float
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(trackHeight.dp)
            .background(if (trackIndex % 2 == 0) Color(0xFF1A1A1A) else Color(0xFF121212))
    ) {
        Box(modifier = Modifier.fillMaxSize().padding(4.dp)) {
            // Track header
            Box(
                modifier = Modifier
                    .width(80.dp)
                    .fillMaxHeight()
                    .background(Color(0xFF0D0D0D))
                    .padding(8.dp)
            ) {
                Text(text = "A${trackIndex + 1}", color = Color.Green.copy(alpha = 0.7f), fontSize = 11.sp, fontWeight = FontWeight.Bold)
            }
            
            // Audio waveform
            Box(modifier = Modifier
                .fillMaxWidth()
                .offset(x = 80.dp)
                .fillMaxHeight()
                .padding(8.dp)
            ) {
                CenteredText("Audio Track ${trackIndex + 1} - Waveform")
            }
        }
    }
}

@Composable
fun RecordViewerPanel(state: EditorState) {
    Card(
        modifier = Modifier
            .width(360.dp)
            .fillMaxHeight()
            .background(Color(0xFF121212))
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Record viewer (program output)
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(16f / 9f)
                    .background(Color.Black)
            ) {
                // This would show the Vulkan render output
                CenteredText("Program Monitor")
            }
            
            Divider(color = Color.White.copy(alpha = 0.1f))
            
            // Inspector (compact)
            InspectorPanel(state)
            
            Divider(color = Color.White.copy(alpha = 0.1f))
            
            // Audio meters
            AudioMetersPanel()
        }
    }
}

@Composable
fun AudioMetersPanel() {
    Column(modifier = Modifier.padding(16.dp)) {
        Text(text = "Audio Meters", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp, fontWeight = FontWeight.Bold)
        androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))
        
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
        ) {
            repeat(2) { channel ->
                Column(
                    modifier = Modifier.weight(1f),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text(text = "Ch ${channel + 1}", color = Color.White.copy(alpha = 0.5f), fontSize = 10.sp)
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(80.dp)
                            .background(Color(0xFF0D0D0D))
                    ) {
                        // Would draw actual meter here
                        CenteredText("📊")
                    }
                    Text(text = "-12 dB", color = Color.Green, fontSize = 10.sp)
                }
            }
        }
    }
}

@Composable
fun PlaybackControlsPanel(state: EditorState) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(100.dp)
            .background(Color(0xFF121212))
            .padding(16.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Timecode display
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(text = formatTimecode(state.currentTimeSeconds), color = Color.White, fontSize = 18.sp, fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace, fontWeight = FontWeight.Bold)
                
                // Transport controls
                Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)) {
                    IconButton(onClick = { state.seek(0.0) }) {
                        Icon(painter = painterResource(id = android.R.drawable.ic_media_previous), contentDescription = "Start", tint = Color.White)
                    }
                    IconButton(onClick = { state.seek(state.currentTimeSeconds - 1.0/30.0) }) {
                        Icon(painter = painterResource(id = android.R.drawable.ic_media_rew), contentDescription = "Frame Back", tint = Color.White)
                    }
                    IconButton(
                        onClick = { state.setPlaying(!state.isPlaying) },
                        modifier = Modifier.size(56.dp),
                        colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                            containerColor = if (state.isPlaying) Color.Cyan else Color.Transparent
                        )
                    ) {
                        Icon(
                            painter = painterResource(id = if (state.isPlaying) android.R.drawable.ic_media_pause else android.R.drawable.ic_media_play),
                            contentDescription = if (state.isPlaying) "Pause" else "Play",
                            tint = if (state.isPlaying) Color.Black else Color.Cyan
                        )
                    }
                    IconButton(onClick = { state.seek(state.currentTimeSeconds + 1.0/30.0) }) {
                        Icon(painter = painterResource(id = android.R.drawable.ic_media_ff), contentDescription = "Frame Forward", tint = Color.White)
                    }
                    IconButton(onClick = { state.seek(state.durationSeconds) }) {
                        Icon(painter = painterResource(id = android.R.drawable.ic_media_next), contentDescription = "End", tint = Color.White)
                    }
                }
                
                Text(text = formatTimecode(state.durationSeconds), color = Color.White.copy(alpha = 0.7f), fontSize = 18.sp, fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace)
            }
            
            // Seek slider
            Slider(
                modifier = Modifier.fillMaxWidth().padding(top = 8.dp),
                value = (state.currentTimeSeconds / state.durationSeconds).coerceIn(0f, 1f),
                onValueChange = { pos ->
                    state.seek(pos * state.durationSeconds)
                },
                colors = androidx.compose.material3.SliderDefaults.colors(
                    thumbColor = Color.Cyan,
                    activeTrackColor = Color.Cyan,
                    inactiveTrackColor = Color.White.copy(alpha = 0.2f)
                )
            )
        }
    }
}

@Composable
fun CenteredText(text: String) {
    Box(
        modifier = Modifier.fillMaxSize(),
        contentAlignment = Alignment.Center
    ) {
        Text(text = text, color = Color.White.copy(alpha = 0.5f), fontSize = 14.sp)
    }
}

private fun formatDuration(seconds: Double): String {
    val totalSeconds = seconds.roundToInt()
    val minutes = totalSeconds / 60
    val secs = totalSeconds % 60
    return String.format("%02d:%02d", minutes, secs)
}

private fun formatTimecode(seconds: Double): String {
    val totalFrames = (seconds * 30).roundToInt()
    val hours = totalFrames / (30 * 60 * 60)
    val minutes = (totalFrames / (30 * 60)) % 60
    val secs = (totalFrames / 30) % 60
    val frames = totalFrames % 30
    return String.format("%02d:%02d:%02d:%02d", hours, minutes, secs, frames)
}