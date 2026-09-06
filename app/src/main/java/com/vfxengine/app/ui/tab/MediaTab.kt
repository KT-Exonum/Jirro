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
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyVerticalGrid
import androidx.compose.foundation.lazy.gridCells
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Card
import androidx.compose.material3.Divider
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
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.vfxengine.app.ui.common.EditorState

/**
 * Media Tab - DaVinci Resolve style Media Pool
 * Full-screen media browser with bins, list/grid view, metadata panel
 */
@Composable
fun MediaTab(state: EditorState) {
    var viewMode by remember { mutableStateOf(ViewMode.Grid) }
    var selectedBin by remember { mutableStateOf<String>("Master") }
    var showMetadata by remember { mutableStateOf(false) }
    var selectedMedia by remember { mutableStateOf<EditorState.MediaFile?>(null) }

    Column(modifier = Modifier.fillMaxSize()) {
        // Top toolbar
        MediaToolbar(
            viewMode = viewMode,
            onViewModeChange = { viewMode = it },
            selectedBin = selectedBin,
            onBinChange = { selectedBin = it },
            showMetadata = showMetadata,
            onMetadataToggle = { showMetadata = !showMetadata }
        )

        Divider(color = Color.White.copy(alpha = 0.1f))

        // Main content
        Row(modifier = Modifier.fillMaxSize().weight(1f)) {
            // Left: Bin list
            BinSidebar(
                bins = listOf("Master", "Video", "Audio", "Images", "Graphics", "Favorites"),
                selectedBin = selectedBin,
                onBinClick = { selectedBin = it }
            )

            Divider(color = Color.White.copy(alpha = 0.1f))

            // Center: Media grid/list
            MediaGrid(
                state = state,
                viewMode = viewMode,
                selectedMedia = selectedMedia,
                onMediaClick = { selectedMedia = it }
            )

            // Right: Metadata/Preview panel
            if (showMetadata) {
                Divider(color = Color.White.copy(alpha = 0.1f))
                MetadataPanel(
                    media = selectedMedia,
                    onClose = { showMetadata = false }
                )
            }
        }

        // Bottom: Preview scrubber for selected media
        if (selectedMedia != null) {
            Divider(color = Color.White.copy(alpha = 0.1f))
            MediaPreviewBar(media = selectedMedia!!)
        }
    }
}

enum class ViewMode { Grid, List }

@Composable
fun MediaToolbar(
    viewMode: ViewMode,
    onViewModeChange: (ViewMode) -> Unit,
    selectedBin: String,
    onBinChange: (String) -> Unit,
    showMetadata: Boolean,
    onMetadataToggle: () -> Unit
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
        // Bin dropdown
        androidx.compose.material3.TextButton(onClick = { /* show bin menu */ }) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_agenda),
                    contentDescription = "Bins"
                )
                androidx.compose.foundation.layout.Box(modifier = Modifier.width(8.dp))
                Text(text = selectedBin, color = Color.White, fontSize = 14.sp)
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_media_play),
                    contentDescription = "Expand"
                )
            }
        }

        androidx.compose.foundation.layout.Box(modifier = Modifier.weight(1f))

        // View mode toggle
        Row {
            IconButton(
                onClick = { onViewModeChange(ViewMode.Grid) },
                modifier = Modifier.padding(4.dp),
                colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                    containerColor = if (viewMode == ViewMode.Grid) Color.Cyan.copy(alpha = 0.2f) else Color.Transparent
                )
            ) {
                Icon(painter = painterResource(id = android.R.drawable.ic_menu_grid), contentDescription = "Grid")
            }
            IconButton(
                onClick = { onViewModeChange(ViewMode.List) },
                modifier = Modifier.padding(4.dp),
                colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                    containerColor = if (viewMode == ViewMode.List) Color.Cyan.copy(alpha = 0.2f) else Color.Transparent
                )
            ) {
                Icon(painter = painterResource(id = android.R.drawable.ic_menu_view), contentDescription = "List")
            }
        }

        androidx.compose.foundation.layout.Box(modifier = Modifier.width(16.dp))

        // Actions
        Row {
            IconButton(onClick = { /* Import media */ }) {
                Icon(painter = painterResource(id = android.R.drawable.ic_menu_upload), contentDescription = "Import")
            }
            IconButton(onClick = { /* New bin */ }) {
                Icon(painter = painterResource(id = android.R.drawable.ic_menu_add), contentDescription = "New Bin")
            }
            IconButton(onClick = onMetadataToggle) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_info_details),
                    contentDescription = showMetadata ? "Hide Metadata" : "Show Metadata"
                )
            }
        }
    }
}

@Composable
fun BinSidebar(bins: List<String>, selectedBin: String, onBinClick: (String) -> Unit) {
    Column(
        modifier = Modifier
            .width(200.dp)
            .fillMaxHeight()
            .background(Color(0xFF0D0D0D))
            .padding(8.dp)
    ) {
        Text(text = "Bins", color = Color.White.copy(alpha = 0.5f), fontSize = 12.sp, fontWeight = FontWeight.Bold, modifier = Modifier.padding(16.dp))
        Divider(color = Color.White.copy(alpha = 0.1f), modifier = Modifier.padding(horizontal = 8.dp))
        
        LazyColumn(verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(4.dp)) {
            items(bins) { bin ->
                androidx.compose.material3.TextButton(
                    onClick = { onBinClick(bin) },
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(40.dp)
                        .padding(horizontal = 8.dp),
                    colors = androidx.compose.material3.ButtonDefaults.textButtonColors(
                        containerColor = if (bin == selectedBin) Color.Cyan.copy(alpha = 0.15f) else Color.Transparent
                    )
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Start
                    ) {
                        Icon(
                            painter = painterResource(id = android.R.drawable.ic_menu_agenda),
                            contentDescription = "",
                            tint = if (bin == selectedBin) Color.Cyan else Color.White.copy(alpha = 0.7f)
                        )
                        androidx.compose.foundation.layout.Box(modifier = Modifier.width(12.dp))
                        Text(text = bin, color = if (bin == selectedBin) Color.Cyan else Color.White, fontSize = 13.sp)
                    }
                }
            }
        }
    }
}

@Composable
fun MediaGrid(
    state: EditorState,
    viewMode: ViewMode,
    selectedMedia: EditorState.MediaFile?,
    onMediaClick: (EditorState.MediaFile) -> Unit
) {
    val mediaList = state.mediaFiles
    
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFF0F0F0F))
            .padding(16.dp)
    ) {
        if (mediaList.isEmpty()) {
            CenteredText("No media in bin. Click Import to add files.")
        } else {
            if (viewMode == ViewMode.Grid) {
                LazyVerticalGrid(
                    cells = gridCells.Fixed(4),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(12.dp),
                    verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(12.dp),
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(0.dp)
                ) {
                    items(mediaList) { media ->
                        MediaGridItem(
                            media = media,
                            isSelected = selectedMedia == media,
                            onClick = { onMediaClick(media) }
                        )
                    }
                }
            } else {
                LazyColumn(verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)) {
                    items(mediaList) { media ->
                        MediaListItem(
                            media = media,
                            isSelected = selectedMedia == media,
                            onClick = { onMediaClick(media) }
                        )
                    }
                }
            }
        }
    }
}

@Composable
fun MediaGridItem(
    media: EditorState.MediaFile,
    isSelected: Boolean,
    onClick: () -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .aspectRatio(16f / 9f)
            .background(if (isSelected) Color(0xFF1A3A4A) else Color(0xFF1E1E1E)),
        onClick = onClick
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Thumbnail placeholder
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .background(Color(0xFF121212))
            ) {
                if (isSelected) {
                    Box(
                        modifier = Modifier
                            .size(48.dp)
                            .align(Alignment.Center),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            painter = painterResource(id = android.R.drawable.ic_media_play),
                            contentDescription = "Play",
                            tint = Color.Cyan
                        )
                    }
                }
            }
            
            // Info
            Column(modifier = Modifier.padding(8.dp)) {
                Text(
                    text = media.name,
                    color = Color.White,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Medium,
                    maxLines = 1,
                    overflow = androidx.compose.ui.text.TextOverflow.Ellipsis
                )
                Text(
                    text = "${media.width}×${media.height} · ${formatDuration(media.duration)}",
                    color = Color.White.copy(alpha = 0.6f),
                    fontSize = 10.sp
                )
            }
        }
    }
}

@Composable
fun MediaListItem(
    media: EditorState.MediaFile,
    isSelected: Boolean,
    onClick: () -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .background(if (isSelected) Color(0xFF1A3A4A) else Color(0xFF1E1E1E)),
        onClick = onClick
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(72.dp)
                .padding(12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Thumbnail
            Box(
                modifier = Modifier
                    .width(128.dp)
                    .height(72.dp)
                    .background(Color(0xFF121212))
            )
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.width(12.dp))
            
            // Info
            Column(modifier = Modifier.weight(1f)) {
                Text(text = media.name, color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Medium, maxLines = 1, overflow = androidx.compose.ui.text.TextOverflow.Ellipsis)
                Text(text = "${media.width}×${media.height} · ${formatDuration(media.duration)}", color = Color.White.copy(alpha = 0.6f), fontSize = 12.sp)
            }
            
            // Duration badge
            Text(text = formatDuration(media.duration), color = Color.White.copy(alpha = 0.5f), fontSize = 12.sp, fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace)
        }
    }
}

@Composable
fun MetadataPanel(media: EditorState.MediaFile?, onClose: () -> Unit) {
    media?.let { m ->
        Card(
            modifier = Modifier
                .width(320.dp)
                .fillMaxHeight()
                .background(Color(0xFF121212))
        ) {
            Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
                ) {
                    Text(text = "Metadata", color = Color.White, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                    IconButton(onClick = onClose) {
                        Icon(painter = painterResource(id = android.R.drawable.ic_menu_close_clear_cancel), contentDescription = "Close")
                    }
                }
                
                Divider(color = Color.White.copy(alpha = 0.1f), modifier = Modifier.padding(vertical = 12.dp))
                
                MetadataRow("Name", m.name)
                MetadataRow("Resolution", "${m.width} × ${m.height}")
                MetadataRow("Duration", formatDuration(m.duration))
                MetadataRow("Frame Rate", "30 fps")
                MetadataRow("Codec", "H.264")
                MetadataRow("Color Space", "Rec.709")
                MetadataRow("Audio", "AAC 48kHz Stereo")
            }
        }
    }
}

@Composable
fun MetadataRow(label: String, value: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 8.dp),
        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
    ) {
        Text(text = label, color = Color.White.copy(alpha = 0.6f), fontSize = 13.sp)
        Text(text = value, color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.Medium)
    }
}

@Composable
fun MediaPreviewBar(media: EditorState.MediaFile) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(120.dp)
            .background(Color(0xFF121212))
            .padding(16.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
            ) {
                Text(text = "Preview: ${media.name}", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Medium)
                Text(text = formatDuration(media.duration), color = Color.White.copy(alpha = 0.6f), fontSize = 12.sp)
            }
            
            // Scrubber
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(40.dp)
                    .padding(top = 8.dp)
            ) {
                // Timeline bar
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(4.dp)
                        .background(Color.White.copy(alpha = 0.2f))
                )
                
                // Playhead
                Box(
                    modifier = Modifier
                        .width(2.dp)
                        .height(20.dp)
                        .background(Color.Cyan)
                        .graphicsLayer { translationX = (200f - 1f).px; translationY = -8.px }
                )
            }
            
            // Transport
            Row(
                modifier = Modifier.fillMaxWidth().padding(top = 8.dp),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Center
            ) {
                IconButton(onClick = { }) {
                    Icon(painter = painterResource(id = android.R.drawable.ic_media_rew), contentDescription = "Rewind")
                }
                IconButton(onClick = { }, modifier = Modifier.size(48.dp)) {
                    Icon(painter = painterResource(id = android.R.drawable.ic_media_play), contentDescription = "Play", tint = Color.Cyan)
                }
                IconButton(onClick = { }) {
                    Icon(painter = painterResource(id = android.R.drawable.ic_media_ff), contentDescription = "Forward")
                }
            }
        }
    }
}

@Composable
fun CenteredText(text: String) {
    Box(
        modifier = Modifier.fillMaxSize(),
        contentAlignment = Alignment.Center
    ) {
        Text(text = text, color = Color.White.copy(alpha = 0.5f), fontSize = 16.sp)
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