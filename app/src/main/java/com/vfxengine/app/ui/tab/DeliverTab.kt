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
import androidx.compose.material3.TextButton
import androidx.compose.material3.FilledTextField
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
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.vfxengine.app.NativeEngine
import com.vfxengine.app.ui.common.EditorState

/**
 * Deliver Tab - Export Settings Screen
 * Preset tabs: YouTube, TikTok, Custom
 * Preview with filmstrip icon
 * File name input
 * 2x2 summary grid
 * Advanced settings dropdown
 * Start export button
 */
@Composable
fun DeliverTab(state: EditorState, engine: NativeEngine) {
    var selectedPreset by remember { mutableStateOf(ExportPreset.YouTube) }
    var fileName by remember { mutableStateOf("My_Video_Edit_v1") }
    var showAdvanced by remember { mutableStateOf(false) }
    var isExporting by remember { mutableStateOf(false) }
    var exportProgress by remember { mutableStateOf(0f) }

    Column(modifier = Modifier.fillMaxSize()) {
        // Preset tabs
        PresetTabs(
            selectedPreset = selectedPreset,
            onPresetChange = { selectedPreset = it }
        )

        // Preview area
        PreviewArea()

        // File name input
        FileNameInput(
            fileName = fileName,
            onFileNameChange = { fileName = it }
        )

        // Summary grid
        SummaryGrid(
            preset = selectedPreset,
            duration = state.durationSeconds
        )

        // Advanced settings dropdown
        AdvancedSettings(
            showAdvanced = showAdvanced,
            onToggle = { showAdvanced = !showAdvanced }
        )

        // Start export button
        StartExportButton(
            isExporting = isExporting,
            progress = exportProgress,
            onClick = { /* Start export */ }
        )
    }
}

enum class ExportPreset(val label: String, val resolution: String, val frameRate: String, val estSize: String) {
    YouTube("YouTube", "1080p", "30 fps", "145 MB"),
    TikTok("TikTok", "1080×1920", "30 fps", "45 MB"),
    Custom("Custom", "1080p", "30 fps", "—")
}

@Composable
fun PresetTabs(
    selectedPreset: ExportPreset,
    onPresetChange: (ExportPreset) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(48.dp)
            .padding(horizontal = 16.dp)
            .background(Color(0xFF121212)),
        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        ExportPreset.values().forEach { preset ->
            androidx.compose.material3.TextButton(
                onClick = { onPresetChange(preset) },
                modifier = Modifier
                    .weight(1f)
                    .height(36.dp)
                    .padding(horizontal = 8.dp),
                colors = androidx.compose.material3.ButtonDefaults.textButtonColors(
                    containerColor = if (selectedPreset == preset) Color.Cyan.copy(alpha = 0.2f) else Color.Transparent
                )
            ) {
                Text(
                    text = preset.label,
                    color = if (selectedPreset == preset) Color.Cyan else Color.White.copy(alpha = 0.7f),
                    fontSize = 13.sp,
                    fontWeight = if (selectedPreset == preset) FontWeight.Bold else FontWeight.Normal
                )
            }
        }
    }
}

@Composable
fun PreviewArea() {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(200.dp)
            .padding(horizontal = 16.dp, vertical = 8.dp)
            .background(Color.Black)
    ) {
        Box(
            modifier = Modifier.fillMaxSize(),
            contentAlignment = Alignment.Center
        ) {
            // Filmstrip icon
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.Center
            ) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_media_play),
                    contentDescription = "Preview",
                    tint = Color.White.copy(alpha = 0.3f),
                    modifier = Modifier.size(64.dp)
                )
                androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))
                Text(
                    text = "Preview",
                    color = Color.White.copy(alpha = 0.4f),
                    fontSize = 14.sp
                )
            }
        }
    }
}

@Composable
fun FileNameInput(
    fileName: String,
    onFileNameChange: (String) -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp)
            .background(Color(0xFF121212))
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(text = "File Name", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp, fontWeight = FontWeight.Bold)
                Text(text = ".mp4", color = Color.White.copy(alpha = 0.5f), fontSize = 12.sp, fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace)
            }

            androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))

            FilledTextField(
                value = fileName,
                onValueChange = onFileNameChange,
                modifier = Modifier.fillMaxWidth(),
                singleLine = true,
                textStyle = androidx.compose.ui.text.TextStyle(color = Color.White, fontSize = 14.sp),
                colors = TextFieldDefaults.filledTextFieldColors(
                    containerColor = Color(0xFF1E1E1E),
                    focusedContainerColor = Color(0xFF2D2D2D),
                    textColor = Color.White,
                    placeholderColor = Color.White.copy(alpha = 0.4f)
                )
            )
        }
    }
}

@Composable
fun SummaryGrid(
    preset: ExportPreset,
    duration: Double
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp)
            .background(Color(0xFF121212))
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            // 2x2 grid
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
            ) {
                SummaryItem("Resolution", preset.resolution)
                SummaryItem("Frame Rate", preset.frameRate)
            }

            androidx.compose.foundation.layout.Box(modifier = Modifier.height(12.dp))

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
            ) {
                SummaryItem("Est. Size", preset.estSize)
                SummaryItem("Duration", formatDuration(duration))
            }
        }
    }
}

@Composable
fun SummaryItem(label: String, value: String) {
    Box(
        modifier = Modifier
            .weight(1f)
            .background(Color(0xFF1E1E1E))
            .padding(16.dp)
    ) {
        Column(
            modifier = Modifier.fillMaxSize(),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = androidx.compose.foundation.layout.Arrangement.Center
        ) {
            Text(text = label, color = Color.White.copy(alpha = 0.5f), fontSize = 11.sp)
            Text(text = value, color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Medium)
        }
    }
}

@Composable
fun AdvancedSettings(
    showAdvanced: Boolean,
    onToggle: () -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp)
            .background(Color(0xFF121212))
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Toggle button
            TextButton(
                onClick = onToggle,
                modifier = Modifier.fillMaxWidth().padding(16.dp).height(48.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "Advanced Settings",
                        color = Color.White.copy(alpha = 0.7f),
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Bold
                    )
                    Icon(
                        painter = painterResource(id = if (showAdvanced) android.R.drawable.ic_media_ff else android.R.drawable.ic_media_rew),
                        contentDescription = "",
                        tint = Color.White.copy(alpha = 0.5f),
                        modifier = Modifier.size(24.dp).rotate(if (showAdvanced) 90f else -90f)
                    )
                }
            }

            // Advanced options (collapsible)
            if (showAdvanced) {
                Divider(color = Color.White.copy(alpha = 0.1f))
                Column(modifier = Modifier.padding(16.dp).fillMaxWidth()) {
                    AdvancedOption("Codec", "H.264 (Main Profile)")
                    AdvancedOption("Bitrate Mode", "VBR")
                    AdvancedOption("Target Bitrate", "15 Mbps")
                    AdvancedOption("Max Bitrate", "20 Mbps")
                    AdvancedOption("Keyframe Interval", "2s (60 frames)")
                    AdvancedOption("Color Space", "Rec.709")
                    AdvancedOption("Audio Codec", "AAC-LC")
                    AdvancedOption("Audio Bitrate", "256 kbps")
                }
                Divider(color = Color.White.copy(alpha = 0.1f))
            }
        }
    }
}

@Composable
fun AdvancedOption(label: String, value: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 8.dp),
        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
    ) {
        Text(text = label, color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
        TextButton(onClick = { /* Show dropdown */ }) {
            Text(text = value, color = Color.White, fontSize = 12.sp)
        }
    }
}

@Composable
fun StartExportButton(
    isExporting: Boolean,
    progress: Float,
    onClick: () -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 16.dp)
            .background(Color(0xFF121212))
    ) {
        if (isExporting) {
            // Exporting progress
            Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
                ) {
                    Text(text = "Exporting...", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                    Text(text = "${(progress * 100).roundToInt()}%", color = Color.Cyan, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                }

                androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))

                androidx.compose.material3.LinearProgressIndicator(
                    modifier = Modifier.fillMaxWidth().height(8.dp),
                    progress = progress,
                    color = Color.Cyan,
                    trackColor = Color.White.copy(alpha = 0.1f)
                )

                androidx.compose.foundation.layout.Box(modifier = Modifier.height(12.dp))

                androidx.compose.material3.TextButton(
                    onClick = { /* Cancel */ },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(text = "Cancel Export", color = Color.Red, fontSize = 13.sp)
                }
            }
        } else {
            // Start export button
            Button(
                onClick = onClick,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(56.dp),
                colors = androidx.compose.material3.ButtonDefaults.buttonColors(containerColor = Color.Cyan)
            ) {
                Text(
                    text = "Start Export",
                    color = Color.Black,
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold
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

fun startExport() {
    // TODO: Implement export
}