package com.vfxengine.app.ui.tab

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.weight
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.vfxengine.app.ui.common.EditorState

/**
 * Edit Tab - Timeline Editor Screen
 * Top: Video preview with timecode
 * Playback controls: Prev/Play/Next on left, Zoom +/- on right
 * Multi-track timeline: V2, V1, A1
 * Quick actions: Split, Trim, Delete
 * Bottom: Selected item info + Open in node editor
 */
@Composable
fun EditTab(state: EditorState) {
    var selectedClipId by remember { mutableStateOf<String?>(null) }

    Column(modifier = Modifier.fillMaxSize()) {
        // Video Preview
        VideoPreview(
            timecode = "${formatTimecode(state.currentTimeSeconds)} / ${formatTimecode(state.durationSeconds)}",
            onPlayClick = { state.setPlaying(!state.isPlaying) }
        )

        // Playback Controls
        PlaybackControlsRow(state = state)

        // Timeline Tracks
        TimelineTracks(
            state = state,
            selectedClipId = selectedClipId,
            onClipClick = { id -> selectedClipId = id }
        )

        // Quick Action Toolbar
        QuickActionToolbar()

        // Selected Item Footer
        val selectedClip = state.clips.value.firstOrNull { it.id == selectedClipId }
        val hasEffects = selectedClip?.let { 
            state.nodes.value[it.nodeId]?.animatedUniforms?.isNotEmpty() ?: false 
        } ?: false
        
        SelectedItemFooter(
            clipName = selectedClip?.id ?: "None",
            hasEffects = hasEffects,
            onNodeEditorClick = { /* Open node editor */ }
        )
    }
}

@Composable
fun VideoPreview(
    timecode: String,
    onPlayClick: () -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(200.dp)
            .background(Color.Black)
    ) {
        // Play button overlay
        Box(
            modifier = Modifier
                .fillMaxSize()
                .align(Alignment.Center)
        ) {
            IconButton(onClick = onPlayClick) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_media_play),
                    contentDescription = "Play",
                    tint = Color.White,
                    modifier = Modifier.size(64.dp)
                )
            }
        }

        // Timecode at bottom right
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp)
                .align(Alignment.BottomEnd)
        ) {
            Text(
                text = timecode,
                color = Color.White,
                fontSize = 14.sp,
                fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace,
                fontWeight = FontWeight.Bold
            )
        }
    }
}

@Composable
fun PlaybackControlsRow(state: EditorState) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(56.dp)
            .background(Color(0xFF121212))
            .padding(horizontal = 16.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxSize(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Left: Previous, Play, Next
            Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)) {
                IconButton(onClick = { state.seek(0.0) }) {
                    Icon(
                        painter = painterResource(id = android.R.drawable.ic_media_previous),
                        contentDescription = "Previous",
                        tint = Color.White
                    )
                }
                IconButton(
                    onClick = { state.setPlaying(!state.isPlaying) },
                    modifier = Modifier.size(48.dp),
                    colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                        containerColor = if (state.isPlaying) Color.Cyan else Color.Transparent
                    )
                ) {
                    Icon(
                        painter = painterResource(id = if (state.isPlaying) android.R.drawable.ic_media_pause else android.R.drawable.ic_media_play),
                        contentDescription = if (state.isPlaying) "Pause" else "Play",
                        tint = if (state.isPlaying) Color.Black else Color.Cyan,
                        modifier = Modifier.size(24.dp)
                    )
                }
                IconButton(onClick = { state.seek(state.durationSeconds) }) {
                    Icon(
                        painter = painterResource(id = android.R.drawable.ic_media_next),
                        contentDescription = "Next",
                        tint = Color.White
                    )
                }
            }

            // Right: Zoom controls
            Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)) {
                Text(text = "Zoom", color = Color.White.copy(alpha = 0.6f), fontSize = 12.sp)
                IconButton(onClick = { state.timeScale = (state.timeScale * 0.8).coerceAtLeast(10.0) }) {
                    Icon(
                        painter = painterResource(id = android.R.drawable.ic_media_rew),
                        contentDescription = "Zoom Out",
                        tint = Color.White
                    )
                }
                IconButton(onClick = { state.timeScale = (state.timeScale * 1.25).coerceAtMost(500.0) }) {
                    Icon(
                        painter = painterResource(id = android.R.drawable.ic_media_ff),
                        contentDescription = "Zoom In",
                        tint = Color.White
                    )
                }
            }
        }
    }
}

@Composable
fun TimelineTracks(
    state: EditorState,
    selectedClipId: String?,
    onClipClick: (String) -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .weight(1f)
            .background(Color(0xFF0F0F0F))
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            // Time ruler
            TimeRuler(state = state)

            androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))

            // Group clips by type and layer
            val videoClips = state.clips.value.filter { it.type == EditorState.ClipType.Video }
            val audioClips = state.clips.value.filter { it.type == EditorState.ClipType.Audio }
            val otherClips = state.clips.value.filter { it.type != EditorState.ClipType.Video && it.type != EditorState.ClipType.Audio }

            // Video tracks (V1, V2, ...)
            val videoLayers = videoClips.groupBy { it.layer }.toSortedMap()
            videoLayers.forEach { (layer, clips) ->
                val trackName = "V${layer + 1}"
                TrackRow(
                    trackName = trackName,
                    clips = clips.map { ClipItem(it.id, it.name, selectedClipId == it.id, true) },
                    selectedClipId = selectedClipId,
                    onClipClick = onClipClick,
                    isVideo = true
                )
                androidx.compose.foundation.layout.Box(modifier = Modifier.height(4.dp))
            }

            // Other visual tracks
            val otherLayers = otherClips.groupBy { it.layer }.toSortedMap()
            otherLayers.forEach { (layer, clips) ->
                val trackName = "L${layer + 1}"
                TrackRow(
                    trackName = trackName,
                    clips = clips.map { ClipItem(it.id, it.name, selectedClipId == it.id, true) },
                    selectedClipId = selectedClipId,
                    onClipClick = onClipClick,
                    isVideo = true
                )
                androidx.compose.foundation.layout.Box(modifier = Modifier.height(4.dp))
            }

            // Audio tracks
            val audioLayers = audioClips.groupBy { it.layer }.toSortedMap()
            audioLayers.forEach { (layer, clips) ->
                val trackName = "A${layer + 1}"
                TrackRow(
                    trackName = trackName,
                    clips = clips.map { ClipItem(it.id, it.name, selectedClipId == it.id, false) },
                    selectedClipId = selectedClipId,
                    onClipClick = onClipClick,
                    isVideo = false
                )
                androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))
            }

            // Empty state if no clips
            if (state.clips.value.isEmpty()) {
                androidx.compose.foundation.layout.Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    Text(text = "No clips on timeline. Add media from Media tab.", color = Color.White.copy(alpha = 0.5f), fontSize = 14.sp)
                }
            }
        }
    }
}

@Composable
fun TimeRuler(state: EditorState) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(24.dp)
            .background(Color(0xFF1A1A1A))
    ) {
        // Simplified time marks
        repeat(6) { i ->
            Box(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxHeight(),
                contentAlignment = Alignment.TopCenter
            ) {
                Text(
                    text = "${i * 30}s",
                    color = Color.White.copy(alpha = 0.5f),
                    fontSize = 10.sp
                )
            }
        }
    }
}

data class ClipItem(val id: String, val name: String, val isSelected: Boolean, val isVideo: Boolean)

@Composable
fun TrackRow(
    trackName: String,
    clips: List<ClipItem>,
    selectedClipId: String?,
    onClipClick: (String) -> Unit,
    isVideo: Boolean
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Start
    ) {
        // Track label
        Box(
            modifier = Modifier
                .width(50.dp)
                .fillMaxHeight()
                .background(Color(0xFF0D0D0D))
                .padding(8.dp),
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = trackName,
                color = if (isVideo) Color.White.copy(alpha = 0.5f) else Color.Green.copy(alpha = 0.7f),
                fontSize = 11.sp,
                fontWeight = FontWeight.Bold
            )
        }

        // Clips
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(if (isVideo) 80.dp else 50.dp)
                .background(Color(0xFF1A1A1A))
        ) {
            Row(
                modifier = Modifier.fillMaxSize(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Start
            ) {
                clips.forEach { clip ->
                    ClipView(
                        clip = clip,
                        onClick = { onClick(clip.id) }
                    )
                }
            }
        }
    }
}

@Composable
fun ClipView(
    clip: ClipItem,
    onClick: () -> Unit
) {
    val color = if (clip.isVideo) Color(0xFF2196F3) else Color.Green

    Card(
        modifier = Modifier
            .width(200.dp)
            .height(if (clip.isVideo) 72.dp else 40.dp)
            .background(if (clip.isSelected) Color(0xFF1A3A4A) else color)
            .padding(4.dp),
        onClick = onClick
    ) {
        Row(
            modifier = Modifier.fillMaxSize().padding(8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Clip type icon
            Icon(
                painter = painterResource(id = if (clip.isVideo) android.R.drawable.ic_media_play else android.R.drawable.ic_media_play),
                contentDescription = "",
                tint = Color.White,
                modifier = Modifier.size(20.dp)
            )

            androidx.compose.foundation.layout.Box(modifier = Modifier.width(8.dp))

            // Clip name
            Text(
                text = clip.name,
                color = Color.White,
                fontSize = 12.sp,
                fontWeight = if (clip.isSelected) FontWeight.Bold else FontWeight.Normal,
                maxLines = 1,
                overflow = androidx.compose.ui.text.TextOverflow.Ellipsis
            )

            // Selected indicator
            if (clip.isSelected) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .width(4.dp)
                        .background(Color.Cyan)
                )
            }
        }
    }
}

@Composable
fun QuickActionToolbar() {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(56.dp)
            .background(Color(0xFF121212))
            .padding(horizontal = 16.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxSize(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            ActionButton("Split", android.R.drawable.ic_menu_crop)
            ActionButton("Trim", android.R.drawable.ic_media_pause)
            ActionButton("Delete", android.R.drawable.ic_menu_delete)
        }
    }
}

@Composable
fun ActionButton(label: String, iconRes: Int) {
    androidx.compose.material3.TextButton(
        onClick = { /* Action */ },
        modifier = Modifier
            .weight(1f)
            .height(40.dp)
            .padding(horizontal = 16.dp),
        colors = androidx.compose.material3.ButtonDefaults.textButtonColors(
            containerColor = Color(0xFF1E1E1E)
        )
    ) {
        Row(
            modifier = Modifier.fillMaxSize(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Center,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(
                painter = painterResource(id = iconRes),
                contentDescription = label,
                tint = Color.White.copy(alpha = 0.7f),
                modifier = Modifier.size(18.dp)
            )
            androidx.compose.foundation.layout.Box(modifier = Modifier.width(8.dp))
            Text(text = label, color = Color.White, fontSize = 13.sp)
        }
    }
}

@Composable
fun SelectedItemFooter(
    clipName: String,
    hasEffects: Boolean,
    onNodeEditorClick: () -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(56.dp)
            .background(Color(0xFF121212))
            .padding(horizontal = 16.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxSize(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Info
            Text(
                text = "Selected: $clipName${if (hasEffects) ", 1 effect applied" else ""}",
                color = Color.White.copy(alpha = 0.8f),
                fontSize = 13.sp
            )

            // Open in node editor button
            androidx.compose.material3.Button(
                onClick = onNodeEditorClick,
                modifier = Modifier.height(40.dp).padding(horizontal = 16.dp),
                colors = androidx.compose.material3.ButtonDefaults.buttonColors(containerColor = Color.Cyan)
            ) {
                Row(
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Icon(
                        painter = painterResource(id = android.R.drawable.ic_menu_manage),
                        contentDescription = "Node",
                        tint = Color.Black,
                        modifier = Modifier.size(18.dp)
                    )
                    Text(text = "Open in node editor", color = Color.Black, fontSize = 13.sp, fontWeight = FontWeight.Medium)
                }
            }
        }
    }
}
private fun formatTimecode(seconds: Double): String {
    val totalFrames = (seconds * 30).roundToInt()
    val hours = totalFrames / (30 * 60 * 60)
    val minutes = (totalFrames / (30 * 60)) % 60
    val secs = (totalFrames / 30) % 60
    val frames = totalFrames % 30
    return String.format("%02d:%02d:%02d:%02d", hours, minutes, secs, frames)
}

