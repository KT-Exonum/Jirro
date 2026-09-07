package com.vfxengine.app.ui.tab

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
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
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt
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
fun EditTab(
    state: EditorState,
    onTabSwitch: (EditorRoot.EditorTab) -> Unit
) {
    var selectedClipId by remember { mutableStateOf<String?>(null) }
    var showTransitionPanel by remember { mutableStateOf(false) }
    var selectedTransitionFrom by remember { mutableStateOf<String?>(null) }
    var selectedTransitionTo by remember { mutableStateOf<String?>(null) }

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
            onClipClick = { clipId, extend ->
                if (extend) {
                    state.selectClip(clipId, true)
                } else {
                    state.selectClip(clipId, false)
                    selectedClipId = clipId
                }
            },
            onClipLongClick = { clipId ->
                // Show context menu or start drag
            },
            onRippleDelete = { clipId ->
                state.rippleDelete(clipId)
                if (selectedClipId == clipId) selectedClipId = null
            }
        )

        // Quick Action Toolbar
        QuickActionToolbar(
            state = state,
            selectedClipId = selectedClipId,
            onTransitionClick = { fromId, toId ->
                selectedTransitionFrom = fromId
                selectedTransitionTo = toId
                showTransitionPanel = true
            }
        )

        // Transition Panel
        if (showTransitionPanel) {
            TransitionPanel(
                state = state,
                fromClipId = selectedTransitionFrom ?: "",
                toClipId = selectedTransitionTo ?: "",
                onDismiss = { showTransitionPanel = false },
                onApply = { type, duration ->
                    selectedTransitionFrom?.let { from ->
                        selectedTransitionTo?.let { to ->
                            state.createTransition(from, to, duration, type.shaderNodeId)
                            showTransitionPanel = false
                        }
                    }
                }
            )
        }

        // Selected Item Footer
        val selectedClip = state.clips.value.firstOrNull { it.id == selectedClipId }
        val hasEffects = selectedClip?.let { 
            state.nodes.value[it.nodeId]?.animatedUniforms?.isNotEmpty() ?: false 
        } ?: false
        val hasProxy = selectedClip?.proxyGenerated ?: false
        val useProxy = selectedClip?.useProxy ?: false
        
        SelectedItemFooter(
            clipName = selectedClip?.id ?: "None",
            hasEffects = hasEffects,
            hasProxy = hasProxy,
            useProxy = useProxy,
            onNodeEditorClick = { onTabSwitch(EditorRoot.EditorTab.Effects) },
            onProxyToggle = { if (selectedClip != null) state.toggleProxy(selectedClip.id) },
            onProxyGenerate = { if (selectedClip != null) state.generateProxy(selectedClip.id) }
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
    onClipClick: (String, Boolean) -> Unit,
    onClipLongClick: (String) -> Unit,
    onRippleDelete: (String) -> Unit
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
                    clips = clips.map { ClipItem(it.id, it.name, selectedClipId == it.id, true, state.selectedClipIds.value.contains(it.id)) },
                    selectedClipId = selectedClipId,
                    multiSelectedIds = state.selectedClipIds.value,
                    onClipClick = onClipClick,
                    onClipLongClick = onClipLongClick,
                    onRippleDelete = onRippleDelete,
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
                    clips = clips.map { ClipItem(it.id, it.name, selectedClipId == it.id, true, state.selectedClipIds.value.contains(it.id)) },
                    selectedClipId = selectedClipId,
                    multiSelectedIds = state.selectedClipIds.value,
                    onClipClick = onClipClick,
                    onClipLongClick = onClipLongClick,
                    onRippleDelete = onRippleDelete,
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
                    clips = clips.map { ClipItem(it.id, it.name, selectedClipId == it.id, false, state.selectedClipIds.value.contains(it.id)) },
                    selectedClipId = selectedClipId,
                    multiSelectedIds = state.selectedClipIds.value,
                    onClipClick = onClipClick,
                    onClipLongClick = onClipLongClick,
                    onRippleDelete = onRippleDelete,
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

data class ClipItem(
    val id: String, 
    val name: String, 
    val isSelected: Boolean, 
    val isVideo: Boolean,
    val isMultiSelected: Boolean = false
)

@Composable
fun TrackRow(
    trackName: String,
    clips: List<ClipItem>,
    selectedClipId: String?,
    multiSelectedIds: Set<String>,
    onClipClick: (String, Boolean) -> Unit, // (clipId, extendSelection)
    onClipLongClick: (String) -> Unit,
    onRippleDelete: (String) -> Unit,
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
                        onClick = { onClick(clip.id, false) },
                        onLongClick = { onLongClick(clip.id) },
                        onRippleDelete = { onRippleDelete(clip.id) },
                        multiSelectedIds = multiSelectedIds,
                        extendSelection = { extend, clipId -> onClick(clipId, extend) }
                    )
                }
            }
        }
    }
}

@Composable
fun ClipView(
    clip: ClipItem,
    onClick: () -> Unit,
    onLongClick: () -> Unit,
    onRippleDelete: () -> Unit,
    multiSelectedIds: Set<String>,
    extendSelection: (Boolean, String) -> Unit
) {
    val color = if (clip.isVideo) Color(0xFF2196F3) else Color.Green
    val isMultiSelected = multiSelectedIds.contains(clip.id)
    val isAnySelected = clip.isSelected || isMultiSelected
    
    val clipColor = when {
        clip.isSelected -> Color(0xFF1A3A4A)
        isMultiSelected -> Color(0xFF3A1A4A) // Purple for multi-select
        else -> color
    }

    Box(
        modifier = Modifier
            .width(200.dp)
            .height(if (clip.isVideo) 72.dp else 40.dp)
            .background(clipColor)
            .padding(4.dp),
        contentAlignment = Alignment.Center
    ) {
        Card(
            modifier = Modifier
                .fillMaxSize()
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
                    fontWeight = if (clip.isSelected || isMultiSelected) FontWeight.Bold else FontWeight.Normal,
                    maxLines = 1,
                    overflow = androidx.compose.ui.text.TextOverflow.Ellipsis
                )

                // Multi-select indicator
                if (isMultiSelected) {
                    androidx.compose.foundation.layout.Box(modifier = Modifier.width(4.dp))
                    Icon(
                        painter = painterResource(id = android.R.drawable.checkbox_on_background),
                        contentDescription = "Multi-selected",
                        tint = Color.Cyan,
                        modifier = Modifier.size(16.dp)
                    )
                }

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
        
        // Ripple delete button on long press
        androidx.compose.material3.IconButton(
            onClick = onRippleDelete,
            modifier = Modifier
                .size(24.dp)
                .align(Alignment.TopEnd)
                .padding(4.dp),
            colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                containerColor = Color.Red.copy(alpha = 0.3f)
            )
        ) {
            Icon(
                painter = painterResource(id = android.R.drawable.ic_menu_delete),
                contentDescription = "Ripple Delete",
                tint = Color.Red,
                modifier = Modifier.size(16.dp)
            )
        }
    }
}

@Composable
fun QuickActionToolbar(
    state: EditorState,
    selectedClipId: String?,
    onTransitionClick: (String, String) -> Unit
) {
    val selectedClip = selectedClipId?.let { state.clips.value.firstOrNull { it.id == it } }
    val hasProxy = selectedClip?.proxyGenerated ?: false
    val useProxy = selectedClip?.useProxy ?: false
    
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
            ActionButton("Split", android.R.drawable.ic_menu_crop) {
                selectedClipId?.let { state.splitClip(it, state.currentTimeSeconds) }
            }
            ActionButton("Trim", android.R.drawable.ic_media_pause) {
                selectedClipId?.let { 
                    val clip = state.clips.value.firstOrNull { it.id == it }
                    clip?.let { state.trimClip(it.id, it.sourceIn, it.sourceOut) }
                }
            }
            ActionButton("Delete", android.R.drawable.ic_menu_delete) {
                selectedClipId?.let { state.removeClip(it) }
            }
            
            // Transition button - enabled when there are at least 2 clips
            val canTransition = state.clips.value.size >= 2
            ActionButton("Transition", android.R.drawable.ic_media_ff, enabled = canTransition) {
                if (canTransition && selectedClipId != null) {
                    // Find adjacent clip
                    val clips = state.clips.value.sortedBy { it.timelineStart }
                    val currentIndex = clips.indexOfFirst { it.id == selectedClipId }
                    if (currentIndex >= 0 && currentIndex < clips.size - 1) {
                        onTransitionClick(selectedClipId, clips[currentIndex + 1].id)
                    }
                }
            }
            
            // Proxy button
            if (hasProxy) {
                androidx.compose.material3.IconButton(
                    onClick = { if (selectedClipId != null) state.toggleProxy(selectedClipId) },
                    modifier = Modifier.size(40.dp),
                    colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                        containerColor = if (useProxy) Color.Cyan.copy(alpha = 0.2f) else Color.Transparent
                    )
                ) {
                    Icon(
                        painter = painterResource(id = android.R.drawable.ic_menu_zoom),
                        contentDescription = if (useProxy) "Disable Proxy" else "Enable Proxy",
                        tint = if (useProxy) Color.Cyan else Color.White.copy(alpha = 0.7f)
                    )
                }
            } else {
                androidx.compose.material3.TextButton(
                    onClick = { if (selectedClipId != null) state.generateProxy(selectedClipId) },
                    modifier = Modifier.height(40.dp).padding(horizontal = 16.dp)
                ) {
                    Text(text = "Generate Proxy", color = Color.Cyan, fontSize = 13.sp)
                }
            }
        }
    }
}

@Composable
fun ActionButton(label: String, iconRes: Int, enabled: Boolean = true, onClick: () -> Unit = {}) {
    androidx.compose.material3.TextButton(
        onClick = if (enabled) onClick else null,
        modifier = Modifier
            .weight(1f)
            .height(40.dp)
            .padding(horizontal = 16.dp),
        colors = androidx.compose.material3.ButtonDefaults.textButtonColors(
            containerColor = if (enabled) Color(0xFF1E1E1E) else Color(0xFF1E1E1E).copy(alpha = 0.5f)
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
                tint = if (enabled) Color.White.copy(alpha = 0.7f) else Color.White.copy(alpha = 0.3f),
                modifier = Modifier.size(18.dp)
            )
            androidx.compose.foundation.layout.Box(modifier = Modifier.width(8.dp))
            Text(text = label, color = if (enabled) Color.White else Color.White.copy(alpha = 0.5f), fontSize = 13.sp)
        }
    }
}

@Composable
fun SelectedItemFooter(
    clipName: String,
    hasEffects: Boolean,
    hasProxy: Boolean = false,
    useProxy: Boolean = false,
    onNodeEditorClick: () -> Unit,
    onProxyToggle: () -> Unit,
    onProxyGenerate: () -> Unit
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
                text = "Selected: $clipName${if (hasEffects) ", 1 effect applied" else ""}${if (hasProxy) " • Proxy: ${if (useProxy) "ON" else "OFF"}" else ""}",
                color = Color.White.copy(alpha = 0.8f),
                fontSize = 13.sp
            )

            // Buttons
            Row(
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Proxy toggle/generate
                if (hasProxy) {
                    androidx.compose.material3.IconButton(
                        onClick = onProxyToggle,
                        modifier = Modifier.size(40.dp),
                        colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                            containerColor = if (useProxy) Color.Cyan.copy(alpha = 0.2f) else Color.Transparent
                        )
                    ) {
                        Icon(
                            painter = painterResource(id = android.R.drawable.ic_menu_zoom),
                            contentDescription = if (useProxy) "Disable Proxy" else "Enable Proxy",
                            tint = if (useProxy) Color.Cyan else Color.White.copy(alpha = 0.7f)
                        )
                    }
                } else {
                    androidx.compose.material3.TextButton(
                        onClick = onProxyGenerate,
                        modifier = Modifier.height(40.dp).padding(horizontal = 16.dp)
                    ) {
                        Text(text = "Generate Proxy", color = Color.Cyan, fontSize = 12.sp)
                    }
                }

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
}
private fun formatTimecode(seconds: Double): String {
    val totalFrames = (seconds * 30).roundToInt()
    val hours = totalFrames / (30 * 60 * 60)
    val minutes = (totalFrames / (30 * 60)) % 60
    val secs = (totalFrames / 30) % 60
    val frames = totalFrames % 30
    return String.format("%02d:%02d:%02d:%02d", hours, minutes, secs, frames)
}

// Transition panel for creating/editing transitions
@Composable
fun TransitionPanel(
    state: EditorState,
    fromClipId: String,
    toClipId: String,
    onDismiss: () -> Unit,
    onApply: (TransitionType, Double) -> Unit
) {
    var selectedType by remember { mutableStateOf(TransitionType.CrossDissolve) }
    var duration by remember { mutableStateOf(1.0) } // seconds
    
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp)
            .background(Color(0xFF1A1A2A))
            .border(BorderStroke(1.dp, Color.Cyan.copy(alpha = 0.5f)))
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            // Header
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "Create Transition",
                    color = Color.White,
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold
                )
                IconButton(onClick = onDismiss) {
                    Icon(
                        painter = painterResource(id = android.R.drawable.ic_menu_close_clear_cancel),
                        contentDescription = "Close",
                        tint = Color.White.copy(alpha = 0.7f)
                    )
                }
            }
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))
            
            // Clip info
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
            ) {
                ClipInfoCard(state, fromClipId, isFrom = true)
                androidx.compose.foundation.layout.Box(modifier = Modifier.width(16.dp))
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_media_ff),
                    contentDescription = "Transition",
                    tint = Color.Cyan,
                    modifier = Modifier.size(24.dp)
                )
                androidx.compose.foundation.layout.Box(modifier = Modifier.width(16.dp))
                ClipInfoCard(state, toClipId, isFrom = false)
            }
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))
            
            // Transition type selector
            Text(text = "Transition Type", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))
            
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
            ) {
                TransitionType.values().forEach { type ->
                    androidx.compose.material3.OutlinedButton(
                        onClick = { selectedType = type },
                        modifier = Modifier
                            .weight(1f)
                            .height(60.dp),
                        colors = androidx.compose.material3.ButtonDefaults.outlinedButtonColors(
                            containerColor = if (selectedType == type) Color.Cyan.copy(alpha = 0.2f) else Color.Transparent,
                            borderColor = if (selectedType == type) Color.Cyan else Color.White.copy(alpha = 0.3f),
                            contentColor = if (selectedType == type) Color.Cyan else Color.White
                        )
                    ) {
                        Column(
                            modifier = Modifier.fillMaxSize(),
                            horizontalAlignment = Alignment.CenterHorizontally,
                            verticalArrangement = androidx.compose.foundation.layout.Arrangement.Center
                        ) {
                            Text(text = type.label, fontSize = 12.sp, fontWeight = if (selectedType == type) FontWeight.Bold else FontWeight.Normal)
                            androidx.compose.foundation.layout.Box(modifier = Modifier.height(4.dp))
                            Text(text = "${type.defaultDuration}s", fontSize = 10.sp, color = Color.White.copy(alpha = 0.6f))
                        }
                    }
                }
            }
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))
            
            // Duration slider
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(text = "Duration: ${String.format("%.1f", duration)}s", color = Color.White, fontSize = 13.sp)
            }
            
            androidx.compose.material3.Slider(
                modifier = Modifier.fillMaxWidth(),
                value = (duration - 0.1) / (5.0 - 0.1),
                onValueChange = { ratio ->
                    duration = 0.1 + ratio * (5.0 - 0.1)
                },
                colors = androidx.compose.material3.SliderDefaults.colors(
                    thumbColor = Color.Cyan,
                    activeTrackColor = Color.Cyan,
                    inactiveTrackColor = Color.White.copy(alpha = 0.2f)
                )
            )
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))
            
            // Apply button
            androidx.compose.material3.Button(
                onClick = { onApply(selectedType, duration) },
                modifier = Modifier
                    .fillMaxWidth()
                    .height(48.dp),
                colors = androidx.compose.material3.ButtonDefaults.buttonColors(containerColor = Color.Cyan)
            ) {
                Text(text = "Apply Transition", color = Color.Black, fontSize = 14.sp, fontWeight = FontWeight.Bold)
            }
        }
    }
}

@Composable
fun ClipInfoCard(state: EditorState, clipId: String, isFrom: Boolean) {
    val clip = state.clips.value.firstOrNull { it.id == clipId }
    val name = clip?.id ?: "Unknown"
    val typeIcon = when (clip?.type) {
        EditorState.ClipType.Video -> android.R.drawable.ic_media_play
        EditorState.ClipType.Audio -> android.R.drawable.ic_media_play
        else -> android.R.drawable.ic_menu_gallery
    }
    
    Box(
        modifier = Modifier
            .weight(1f)
            .height(60.dp)
            .background(Color(0xFF1E1E1E))
            .padding(12.dp)
    ) {
        Column(
            modifier = Modifier.fillMaxSize(),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = androidx.compose.foundation.layout.Arrangement.Center
        ) {
            Text(text = if (isFrom) "From" else "To", color = Color.White.copy(alpha = 0.5f), fontSize = 10.sp)
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(4.dp))
            Row(
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Center,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    painter = painterResource(id = typeIcon),
                    contentDescription = "",
                    tint = Color.White.copy(alpha = 0.7f),
                    modifier = Modifier.size(16.dp)
                )
                androidx.compose.foundation.layout.Box(modifier = Modifier.width(4.dp))
                Text(text = name, color = Color.White, fontSize = 12.sp, maxLines = 1, overflow = androidx.compose.ui.text.TextOverflow.Ellipsis)
            }
        }
    }
}

enum class TransitionType(val label: String, val shaderNodeId: String, val defaultDuration: Double) {
    CrossDissolve("Cross Dissolve", "blend_crossdissolve", 1.0),
    DipToColor("Dip to Color", "blend_diptocolor", 1.5),
    Slide("Slide", "blend_slide", 0.8),
    Push("Push", "blend_push", 0.8),
    Wipe("Wipe", "blend_wipe", 1.0),
    Zoom("Zoom", "blend_zoom", 1.2)
}

