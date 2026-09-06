package com.vfxengine.app.ui.media

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Card
import androidx.compose.material3.Divider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.vfxengine.app.ui.common.EditorState

/**
 * Media browser for video/image assets
 */
@Composable
fun MediaBrowser(state: EditorState) {
    Column(
        modifier = Modifier
            .fillMaxHeight()
            .width(300.dp)
            .background(Color(0xFF1E1E1E))
    ) {
        // Toolbar
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(48.dp)
                .padding(16.dp)
                .background(Color(0xFF121212))
        ) {
            Text(text = "Media", color = Color.White, fontSize = 16.sp, fontWeight = FontWeight.Bold)
            androidx.compose.foundation.layout.Box(modifier = Modifier.weight(1f))
            androidx.compose.material3.IconButton(onClick = { state.loadMediaFiles() }) {
                androidx.compose.material3.Icon(
                    painter = androidx.compose.ui.res.painterResource(id = android.R.drawable.ic_popup_sync),
                    contentDescription = "Refresh"
                )
            }
        }
        
        Divider(color = Color.White.copy(alpha = 0.1f))
        
        // Media list
        if (state.mediaFiles.isEmpty()) {
            Box(
                modifier = Modifier.fillMaxSize(),
                contentAlignment = Alignment.Center
            ) {
                Text(text = "No media files found", color = Color.White.copy(alpha = 0.5f), fontSize = 14.sp)
            }
        } else {
            LazyColumn(
                modifier = Modifier.fillMaxSize().padding(8.dp),
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
            ) {
                items(state.mediaFiles) { media ->
                    MediaItem(
                        media = media,
                        isSelected = state.selectedMediaFile == media,
                        onClick = { state.selectedMediaFile = media },
                        onDragStart = { /* Handle drag to timeline */ }
                    )
                }
            }
        }
    }
}

@Composable
fun MediaItem(
    media: EditorState.MediaFile,
    isSelected: Boolean,
    onClick: () -> Unit,
    onDragStart: () -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .background(if (isSelected) Color(0xFF3D3D3D) else Color(0xFF2D2D2D)),
        onClick = onClick
    ) {
        Column(modifier = Modifier.padding(12.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
            ) {
                Text(text = media.name, color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold, maxLines = 1, overflow = androidx.compose.ui.text.TextOverflow.Ellipsis)
            }
            
            Row(
                modifier = Modifier.fillMaxWidth().padding(top = 4.dp),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
            ) {
                Text(
                    text = "${media.width}x${media.height} • ${formatDuration(media.duration)}",
                    color = Color.White.copy(alpha = 0.7f),
                    fontSize = 11.sp
                )
            }
        }
    }
}

private fun formatDuration(seconds: Double): String {
    val totalSeconds = seconds.roundToInt()
    val hours = totalSeconds / 3600
    val minutes = (totalSeconds % 3600) / 60
    val secs = totalSeconds % 60
    return if (hours > 0) String.format("%02d:%02d:%02d", hours, minutes, secs)
    else String.format("%02d:%02d", minutes, secs)
}