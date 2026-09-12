package com.vfxengine.app.ui.tab

import android.content.Intent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
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
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.vfxengine.app.ui.common.EditorState
import kotlin.math.roundToInt

/**
 * Media Tab - Simple Media Library Screen
 * Top: Search bar + Add button
 * Filter chips: All, Video, Audio, Graphics
 * 2x2 grid of media assets
 * Bottom: Asset details panel when selected
 */
@Composable
fun MediaTab(
    state: EditorState,
    onImport: () -> Unit
) {
    var searchText by remember { mutableStateOf("") }
    var selectedFilter by remember { mutableStateOf(FilterType.All) }
    var selectedMedia by remember { mutableStateOf<EditorState.MediaFile?>(null) }

    Column(modifier = Modifier.fillMaxSize()) {
        // Top bar: Search + Add
        MediaTopBar(
            searchText = searchText,
            onSearchChange = { searchText = it },
            onAddClick = { onImport() }
        )

        // Filter chips
        FilterChipsRow(
            selectedFilter = selectedFilter,
            onFilterChange = { selectedFilter = it }
        )

        // Media grid
        MediaGrid(
            state = state,
            filter = selectedFilter,
            selectedMedia = selectedMedia,
            onMediaClick = { selectedMedia = it }
        )

        // Asset details panel at bottom
        if (selectedMedia != null) {
            AssetDetailsPanel(
                media = selectedMedia!!,
                onClose = { selectedMedia = null },
                onPreview = { },
                onAdd = { },
                onAddToTimeline = { }
            )
        }
    }
}

enum class FilterType(val label: String) {
    All("All"),
    Video("Video"),
    Audio("Audio"),
    Graphics("Graphics")
}

@Composable
fun MediaTopBar(
    searchText: String,
    onSearchChange: (String) -> Unit,
    onAddClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(56.dp)
            .padding(horizontal = 16.dp)
            .background(Color(0xFF121212)),
        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Search field
        Box(
            modifier = Modifier
                .weight(1f)
                .height(40.dp)
        ) {
            TextField(
                value = searchText,
                onValueChange = onSearchChange,
                modifier = Modifier
                    .fillMaxSize()
                    .padding(horizontal = 12.dp),
                singleLine = true,
                textStyle = androidx.compose.ui.text.TextStyle(color = Color.White, fontSize = 14.sp),
                colors = TextFieldDefaults.colors(
                    focusedTextColor = Color.White,
                    unfocusedTextColor = Color.White,
                    focusedContainerColor = Color(0xFF2D2D2D),
                    unfocusedContainerColor = Color(0xFF1E1E1E),
                    cursorColor = Color.Cyan
                ),
                placeholder = { Text(text = "Search media", color = Color.White.copy(alpha = 0.4f), fontSize = 14.sp) },
                leadingIcon = {
                    Icon(
                        painter = painterResource(id = android.R.drawable.ic_menu_search),
                        contentDescription = "Search",
                        tint = Color.White.copy(alpha = 0.5f)
                    )
                }
            )
        }

        androidx.compose.foundation.layout.Box(modifier = Modifier.width(12.dp))

        // Add button
        IconButton(onClick = onAddClick, modifier = Modifier.size(40.dp)) {
            Icon(
                painter = painterResource(id = android.R.drawable.ic_menu_add),
                contentDescription = "Import",
                tint = Color.Cyan,
                modifier = Modifier.size(24.dp)
            )
        }
    }
}

@Composable
fun FilterChipsRow(
    selectedFilter: FilterType,
    onFilterChange: (FilterType) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp)
            .background(Color(0xFF0F0F0F)),
        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
    ) {
        FilterType.values().forEach { filter ->
            FilterChip(
                selected = filter == selectedFilter,
                onClick = { onFilterChange(filter) },
                modifier = Modifier.height(32.dp),
                label = { Text(text = filter.label, fontSize = 12.sp) },
                colors = FilterChipDefaults.filterChipColors(
                    containerColor = Color(0xFF1E1E1E),
                    labelColor = Color.White,
                    selectedContainerColor = Color.Cyan.copy(alpha = 0.2f),
                    selectedLabelColor = Color.Cyan
                )
            )
        }
    }
}

@Composable
fun ColumnScope.MediaGrid(
    state: EditorState,
    filter: FilterType,
    selectedMedia: EditorState.MediaFile?,
    onMediaClick: (EditorState.MediaFile) -> Unit
) {
    val filteredMedia = state.mediaFiles.filter { media ->
        when (filter) {
            FilterType.All -> true
            FilterType.Video -> media.name.contains(".mp4") || media.name.contains(".mov") || media.name.contains(".avi")
            FilterType.Audio -> media.name.contains(".mp3") || media.name.contains(".wav") || media.name.contains(".aac")
            FilterType.Graphics -> media.name.contains(".png") || media.name.contains(".jpg") || media.name.contains(".jpeg") || media.name.contains(".svg")
        }
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .weight(1f)
            .background(Color(0xFF0F0F0F))
            .padding(16.dp)
    ) {
        if (filteredMedia.isEmpty()) {
            androidx.compose.foundation.layout.Box(
                modifier = Modifier.fillMaxSize(),
                contentAlignment = Alignment.Center
            ) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Icon(
                        painter = painterResource(id = android.R.drawable.ic_menu_gallery),
                        contentDescription = "",
                        tint = Color.White.copy(alpha = 0.3f),
                        modifier = Modifier.size(48.dp)
                    )
                    androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))
                    Text(text = "No media found", color = Color.White.copy(alpha = 0.5f), fontSize = 16.sp)
                }
            }
        } else {
            LazyVerticalGrid(
                columns = GridCells.Fixed(2),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(12.dp),
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(12.dp),
                contentPadding = androidx.compose.foundation.layout.PaddingValues(0.dp)
            ) {
                items(filteredMedia) { media ->
                    MediaGridItem(
                        media = media,
                        isSelected = selectedMedia == media,
                        onClick = { onMediaClick(media) }
                    )
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
    val isVideo = media.name.contains(".mp4") || media.name.contains(".mov") || media.name.contains(".avi")
    val isAudio = media.name.contains(".mp3") || media.name.contains(".wav") || media.name.contains(".aac")
    val isImage = media.name.contains(".png") || media.name.contains(".jpg") || media.name.contains(".jpeg")

    Card(
        modifier = Modifier
            .fillMaxWidth()
            .aspectRatio(16f / 9f)
            .background(if (isSelected) Color(0xFF1A3A4A) else Color(0xFF1E1E1E)),
        onClick = onClick
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Thumbnail area
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .background(Color(0xFF121212))
            ) {
                // Thumbnail image (if available)
                // Icon based on media type
                    Box(
                        modifier = Modifier
                            .size(48.dp)
                            .align(Alignment.Center)
                    ) {
                        Icon(
                            painter = painterResource(id = when {
                                isVideo -> android.R.drawable.ic_media_play
                                isAudio -> android.R.drawable.ic_media_play
                                isImage -> android.R.drawable.ic_menu_gallery
                                else -> android.R.drawable.ic_menu_add
                            }),
                            contentDescription = "Media type",
                            tint = Color.White.copy(alpha = 0.6f),
                            modifier = Modifier.size(48.dp)
                        )
                    }

                // Selected overlay
                if (isSelected) {
                    Box(
                        modifier = Modifier
                            .fillMaxSize()
                            .background(Color.Cyan.copy(alpha = 0.1f))
                    )
                }
            }

            // Info
            Column(modifier = Modifier.padding(12.dp)) {
                Text(
                    text = media.name,
                    color = Color.White,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Medium,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
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
fun AssetDetailsPanel(
    media: EditorState.MediaFile,
    onClose: () -> Unit,
    onPreview: () -> Unit,
    onAdd: () -> Unit,
    onAddToTimeline: (EditorState.MediaFile) -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(140.dp)
            .background(Color(0xFF121212))
            .padding(horizontal = 16.dp, vertical = 8.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            // Title row
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(text = media.name, color = Color.White, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                IconButton(onClick = onClose) {
                    Icon(
                        painter = painterResource(id = android.R.drawable.ic_menu_close_clear_cancel),
                        contentDescription = "Close",
                        tint = Color.White.copy(alpha = 0.7f)
                    )
                }
            }

            // Metadata row
            Row(
                modifier = Modifier.fillMaxWidth().padding(top = 8.dp),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(24.dp)
            ) {
                MetadataItem("Resolution", "${media.width} × ${media.height}")
                MetadataItem("Frame Rate", "59.94 fps")
                MetadataItem("Duration", formatDuration(media.duration))
            }

            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))

            // Action buttons
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(12.dp)
            ) {
                // Preview button
                OutlinedButton(
                    onClick = onPreview,
                    modifier = Modifier.weight(1f).height(44.dp),
                    colors = ButtonDefaults.outlinedButtonColors(
                        contentColor = Color.Cyan
                    )
                ) {
                    Text(text = "Preview", fontSize = 14.sp, fontWeight = FontWeight.Medium)
                }

                // Add button
                Button(
                    onClick = { 
                        onAdd()
                        onAddToTimeline(media)
                    },
                    modifier = Modifier.weight(1f).height(44.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = Color.Cyan)
                ) {
                    Text(text = "+ Add", color = Color.Black, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                }
            }
        }
    }
}

@Composable
fun MetadataItem(label: String, value: String) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(text = label, color = Color.White.copy(alpha = 0.5f), fontSize = 10.sp)
        Text(text = value, color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Medium)
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