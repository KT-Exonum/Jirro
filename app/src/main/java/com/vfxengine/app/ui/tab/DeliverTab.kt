package com.vfxengine.app.ui.tab

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Card
import androidx.compose.material3.Checkbox
import androidx.compose.material3.Divider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
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
import com.vfxengine.app.NativeEngine
import com.vfxengine.app.ui.common.EditorState

/**
 * Deliver Tab - DaVinci Resolve style Deliver page
 * Render queue, presets, export settings, queue management
 */
@Composable
fun DeliverTab(state: EditorState, engine: NativeEngine) {
    var selectedPreset by remember { mutableStateOf(ExportPreset.H264_1080p) }
    var renderQueue by remember { mutableStateOf<List<RenderJob>>(mockRenderJobs()) }
    var isRendering by remember { mutableStateOf(false) }
    var currentJobIndex by remember { mutableStateOf(0) }
    var overallProgress by remember { mutableStateOf(0f) }
    
    Column(modifier = Modifier.fillMaxSize()) {
        // Top toolbar
        DeliverToolbar(
            onAddToQueue = { /* add current timeline to queue */ },
            onStartRender = { isRendering = true; startRender() },
            onStopRender = { isRendering = false },
            isRendering = isRendering
        )
        
        Divider(color = Color.White.copy(alpha = 0.1f))
        
        // Main content
        Row(modifier = Modifier.fillMaxSize().weight(1f)) {
            // Left: Render queue
            RenderQueuePanel(
                jobs = renderQueue,
                currentJobIndex = currentJobIndex,
                onJobClick = { job -> /* select job */ },
                onJobRemove = { job -> renderQueue = renderQueue.filter { it != job } }
            )
            
            Divider(color = Color.White.copy(alpha = 0.1f))
            
            // Center: Preview + Timeline (for selected job)
            DeliverPreviewPanel(state)
            
            Divider(color = Color.White.copy(alpha = 0.1f))
            
            // Right: Export settings
            ExportSettingsPanel(
                preset = selectedPreset,
                onPresetChange = { selectedPreset = it },
                onCustomSettingsChange = { /* update custom settings */ }
            )
        }
        
        // Bottom: Overall progress bar (when rendering)
        if (isRendering) {
            Divider(color = Color.White.copy(alpha = 0.1f))
            RenderProgressBar(
                overallProgress = overallProgress,
                currentJob = if (currentJobIndex < renderQueue.size) renderQueue[currentJobIndex] else null,
                onCancel = { isRendering = false }
            )
        }
    }
}

@Composable
fun DeliverToolbar(
    onAddToQueue: () -> Unit,
    onStartRender: () -> Unit,
    onStopRender: () -> Unit,
    isRendering: Boolean
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
        Text(text = "Deliver", color = Color.White, fontSize = 18.sp, fontWeight = FontWeight.Bold)
        
        androidx.compose.foundation.layout.Box(modifier = Modifier.weight(1f))
        
        Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)) {
            androidx.compose.material3.TextButton(onClick = onAddToQueue) {
                Text(text = "Add to Render Queue", color = Color.Cyan)
            }
            
            if (isRendering) {
                androidx.compose.material3.Button(
                    onClick = onStopRender,
                    colors = androidx.compose.material3.ButtonDefaults.buttonColors(containerColor = Color.Red.copy(alpha = 0.2f))
                ) {
                    Text(text = "Stop Render", color = Color.Red)
                }
            } else {
                androidx.compose.material3.Button(
                    onClick = onStartRender,
                    colors = androidx.compose.material3.ButtonDefaults.buttonColors(containerColor = Color.Cyan)
                ) {
                    Text(text = "Start Render", color = Color.Black)
                }
            }
        }
    }
}

@Composable
fun RenderQueuePanel(
    jobs: List<RenderJob>,
    currentJobIndex: Int,
    onJobClick: (RenderJob) -> Unit,
    onJobRemove: (RenderJob) -> Unit
) {
    Card(
        modifier = Modifier
            .width(350.dp)
            .fillMaxHeight()
            .background(Color(0xFF121212))
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Queue header
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(48.dp)
                    .padding(horizontal = 16.dp)
                    .background(Color(0xFF0D0D0D)),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(text = "Render Queue (${jobs.size})", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                androidx.compose.material3.IconButton(onClick = { /* clear queue */ }) {
                    Icon(painter = painterResource(id = android.R.drawable.ic_menu_delete), contentDescription = "Clear Queue", tint = Color.White.copy(alpha = 0.7f))
                }
            }
            
            Divider(color = Color.White.copy(alpha = 0.1f))
            
            // Job list
            if (jobs.isEmpty()) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Icon(painter = painterResource(id = android.R.drawable.ic_menu_add), contentDescription = "", tint = Color.White.copy(alpha = 0.3f), modifier = Modifier.size(48.dp))
                        Text(text = "Queue is empty", color = Color.White.copy(alpha = 0.5f), fontSize = 14.sp, modifier = Modifier.padding(top = 8.dp))
                        Text(text = "Add timeline to queue to start rendering", color = Color.White.copy(alpha = 0.3f), fontSize = 12.sp)
                    }
                }
            } else {
                LazyColumn(
                    modifier = Modifier.fillMaxSize().padding(8.dp),
                    verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
                ) {
                    itemsIndexed(jobs) { index, job ->
                        RenderJobItem(
                            job = job,
                            index = index,
                            isCurrent = index == currentJobIndex,
                            onClick = { onJobClick(job) },
                            onRemove = { onJobRemove(job) }
                        )
                    }
                }
            }
        }
    }
}

@Composable
fun RenderJobItem(
    job: RenderJob,
    index: Int,
    isCurrent: Boolean,
    onClick: () -> Unit,
    onRemove: () -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .background(if (isCurrent) Color(0xFF1A3A2A) else Color(0xFF1E1E1E))
            .padding(8.dp),
        onClick = onClick
    ) {
        Column(modifier = Modifier.padding(12.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(text = "${index + 1}. ${job.name}", color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.Bold)
                if (isCurrent) {
                    Text(text = "RENDERING", color = Color.Cyan, fontSize = 10.sp, fontWeight = FontWeight.Bold)
                }
            }
            
            Row(
                modifier = Modifier.fillMaxWidth().padding(top = 4.dp),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
            ) {
                Text(text = "${job.preset.label} • ${job.resolution} • ${job.codec}", color = Color.White.copy(alpha = 0.7f), fontSize = 11.sp)
                Text(text = "${job.duration}s", color = Color.White.copy(alpha = 0.5f), fontSize = 11.sp, fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace)
            }
            
            if (job.progress > 0f) {
                androidx.compose.foundation.layout.Box(modifier = Modifier.fillMaxWidth().padding(top = 8.dp))
                androidx.compose.material3.LinearProgressIndicator(
                    modifier = Modifier.fillMaxWidth(),
                    progress = job.progress,
                    color = Color.Cyan,
                    trackColor = Color.White.copy(alpha = 0.1f)
                )
            }
            
            // Remove button
            androidx.compose.foundation.layout.Box(modifier = Modifier.fillMaxWidth().padding(top = 8.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.End
            ) {
                androidx.compose.material3.IconButton(onClick = onRemove) {
                    Icon(painter = painterResource(id = android.R.drawable.ic_menu_close_clear_cancel), contentDescription = "Remove", tint = Color.White.copy(alpha = 0.5f))
                }
            }
        }
    }
}

@Composable
fun DeliverPreviewPanel(state: EditorState) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .fillMaxHeight()
            .weight(1f)
            .background(Color(0xFF121212))
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Preview viewer
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .background(Color.Black)
            ) {
                // Would show render preview here
                CenteredText("Render Preview")
            }
            
            Divider(color = Color.White.copy(alpha = 0.1f))
            
            // Mini timeline for selected job
            MiniTimeline(state)
        }
    }
}

@Composable
fun MiniTimeline(state: EditorState) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(120.dp)
            .background(Color(0xFF0D0D0D))
            .padding(8.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(8.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
            ) {
                Text(text = "Timeline", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
                Text(text = formatDuration(state.durationSeconds), color = Color.White.copy(alpha = 0.5f), fontSize = 11.sp, fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace)
            }
            
            // Mini track view
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .fillMaxHeight()
                    .background(Color(0xFF000000))
            ) {
                CenteredText("Mini Timeline View")
            }
            
            // Timecode
            Row(
                modifier = Modifier.fillMaxWidth().padding(top = 4.dp),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
            ) {
                Text(text = "00:00:00:00", color = Color.White, fontSize = 10.sp, fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace)
                Text(text = formatTimecode(state.durationSeconds), color = Color.White.copy(alpha = 0.5f), fontSize = 10.sp, fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace)
            }
        }
    }
}

@Composable
fun ExportSettingsPanel(
    preset: ExportPreset,
    onPresetChange: (ExportPreset) -> Unit,
    onCustomSettingsChange: () -> Unit
) {
    Card(
        modifier = Modifier
            .width(360.dp)
            .fillMaxHeight()
            .background(Color(0xFF121212))
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Preset selector
            Text(text = "Render Settings", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp, fontWeight = FontWeight.Bold, modifier = Modifier.padding(16.dp))
            
            androidx.compose.foundation.lazy.LazyColumn(
                modifier = Modifier.fillMaxSize().padding(horizontal = 8.dp),
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
            ) {
                items(ExportPreset.values()) { p ->
                    PresetItem(
                        preset = p,
                        isSelected = preset == p,
                        onClick = { onPresetChange(p) }
                    )
                }
            }
            
            Divider(color = Color.White.copy(alpha = 0.1f), modifier = Modifier.padding(horizontal = 16.dp))
            
            // Custom settings (when "Custom" preset selected)
            if (preset == ExportPreset.Custom) {
                CustomExportSettings(onChange = onCustomSettingsChange)
            }
            
            Divider(color = Color.White.copy(alpha = 0.1f), modifier = Modifier.padding(horizontal = 16.dp))
            
            // File output settings
            FileOutputSettings()
            
            Divider(color = Color.White.copy(alpha = 0.1f), modifier = Modifier.padding(horizontal = 16.dp))
            
            // Audio settings
            AudioExportSettings()
        }
    }
}

@Composable
fun PresetItem(
    preset: ExportPreset,
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
                .padding(16.dp)
                .height(60.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(
                modifier = Modifier
                    .width(80.dp)
                    .height(60.dp)
                    .background(Color(0xFF0D0D0D))
            ) {
                CenteredText(preset.icon)
            }
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.width(12.dp))
            
            Column(modifier = Modifier.weight(1f), verticalArrangement = androidx.compose.foundation.layout.Arrangement.Center) {
                Text(text = preset.label, color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                Text(text = preset.description, color = Color.White.copy(alpha = 0.6f), fontSize = 11.sp)
            }
            
            if (isSelected) {
                Icon(painter = painterResource(id = android.R.drawable.ic_media_play), contentDescription = "Selected", tint = Color.Cyan)
            }
        }
    }
}

@Composable
fun CustomExportSettings(onChange: () -> Unit) {
    Column(modifier = Modifier.padding(16.dp)) {
        Text(text = "Custom Settings", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
        
        // Resolution
        SettingRow(
            label = "Resolution",
            content = {
                Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)) {
                    androidx.compose.material3.OutlinedButton(onClick = { }) { Text("1920×1080") }
                    androidx.compose.material3.OutlinedButton(onClick = { }) { Text("3840×2160") }
                    androidx.compose.material3.OutlinedButton(onClick = { }) { Text("Custom") }
                }
            }
        )
        
        // Frame rate
        SettingRow(
            label = "Frame Rate",
            content = {
                Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)) {
                    androidx.compose.material3.OutlinedButton(onClick = { }) { Text("24 fps") }
                    androidx.compose.material3.OutlinedButton(onClick = { }) { Text("30 fps") }
                    androidx.compose.material3.OutlinedButton(onClick = { }) { Text("60 fps") }
                }
            }
        )
        
        // Quality slider
        var quality by remember { mutableStateOf(0.8f) }
        SettingRow(
            label = "Quality",
            content = {
                Slider(
                    modifier = Modifier.fillMaxWidth(),
                    value = quality,
                    onValueChange = { quality = it; onChange() },
                    colors = androidx.compose.material3.SliderDefaults.colors(thumbColor = Color.Cyan, activeTrackColor = Color.Cyan)
                )
            }
        )
    }
}

@Composable
fun FileOutputSettings() {
    Column(modifier = Modifier.padding(16.dp)) {
        Text(text = "File Output", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
        
        SettingRow(
            label = "Output Path",
            content = {
                androidx.compose.material3.FilledTextField(
                    value = "/storage/emulated/0/Movies/render.mp4",
                    onValueChange = { },
                    modifier = Modifier.fillMaxWidth(),
                    singleLine = true,
                    textStyle = androidx.compose.ui.text.TextStyle(color = Color.White, fontSize = 12.sp)
                )
            }
        )
        
        SettingRow(
            label = "Filename Pattern",
            content = {
                androidx.compose.material3.FilledTextField(
                    value = "project_${date}_${time}",
                    onValueChange = { },
                    modifier = Modifier.fillMaxWidth(),
                    singleLine = true,
                    textStyle = androidx.compose.ui.text.TextStyle(color = Color.White, fontSize = 12.sp)
                )
            }
        )
    }
}

@Composable
fun AudioExportSettings() {
    Column(modifier = Modifier.padding(16.dp)) {
        Text(text = "Audio", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
        
        var exportAudio by remember { mutableStateOf(true) }
        SettingRow(
            label = "Export Audio",
            content = {
                Switch(checked = exportAudio, onCheckedChange = { exportAudio = it }, colors = androidx.compose.material3.SwitchDefaults.colors(thumbColor = Color.Cyan))
            }
        )
        
        SettingRow(
            label = "Codec",
            content = {
                androidx.compose.material3.TextButton(onClick = { /* show menu */ }) {
                    Text(text = "AAC 256 kbps", color = Color.White)
                }
            }
        )
        
        SettingRow(
            label = "Sample Rate",
            content = {
                androidx.compose.material3.TextButton(onClick = { }) {
                    Text(text = "48 kHz", color = Color.White)
                }
            }
        )
    }
}

@Composable
fun SettingRow(label: String, content: @Composable () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 12.dp, horizontal = 16.dp),
        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(text = label, color = Color.White.copy(alpha = 0.8f), fontSize = 12.sp)
        content()
    }
}

@Composable
fun RenderProgressBar(
    overallProgress: Float,
    currentJob: RenderJob?,
    onCancel: () -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(80.dp)
            .background(Color(0xFF121212))
            .padding(16.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
            ) {
                Text(text = "Rendering...", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                Text(text = "${(overallProgress * 100).roundToInt()}%", color = Color.Cyan, fontSize = 14.sp, fontWeight = FontWeight.Bold)
            }
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.fillMaxWidth().padding(top = 8.dp))
            
            androidx.compose.material3.LinearProgressIndicator(
                modifier = Modifier.fillMaxWidth(),
                progress = overallProgress,
                color = Color.Cyan,
                trackColor = Color.White.copy(alpha = 0.1f)
            )
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.fillMaxWidth().padding(top = 8.dp))
            
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
            ) {
                currentJob?.let { job ->
                    Text(text = "Job: ${job.name}", color = Color.White.copy(alpha = 0.7f), fontSize = 11.sp)
                }
                androidx.compose.material3.TextButton(onClick = onCancel) {
                    Text(text = "Cancel", color = Color.Red)
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
        Text(text = text, color = Color.White.copy(alpha = 0.5f), fontSize = 14.sp)
    }
}

data class RenderJob(
    val name: String,
    val preset: ExportPreset,
    val resolution: String,
    val codec: String,
    val duration: Double,
    var progress: Float = 0f
)

enum class ExportPreset(val label: String, val description: String, val icon: String) {
    H264_1080p("H.264 1080p", "High quality MP4 for web", "📹"),
    H264_4K("H.264 4K", "Ultra HD for YouTube", "📺"),
    HEVC_1080p("HEVC 1080p", "Smaller file, high quality", "📦"),
    HEVC_4K("HEVC 4K", "Best compression for 4K", "📦"),
    ProRes_422("ProRes 422", "Professional editing codec", "🎬"),
    ProRes_422_HQ("ProRes 422 HQ", "High quality ProRes", "🎬"),
    DNxHR_HQ("DNxHR HQ", "Avid editing codec", "🎬"),
    Custom("Custom", "Manual settings", "⚙️")
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

fun mockRenderJobs(): List<RenderJob> {
    return listOf(
        RenderJob("Main_Edit_v01", ExportPreset.H264_1080p, "1920×1080", "H.264", 45.0, 1.0f),
        RenderJob("Main_Edit_v01_4K", ExportPreset.HEVC_4K, "3840×2160", "HEVC", 45.0, 0.65f),
        RenderJob("Social_Cut_15s", ExportPreset.H264_1080p, "1080×1920", "H.264", 15.0, 0.0f)
    )
}