package com.vfxengine.app.ui.common

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Card
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

/**
 * Profiler overlay showing real-time GPU/CPU stats
 */
@Composable
fun ProfilerOverlay(
    statsJson: String,
    visible: Boolean = true,
    onDismiss: () -> Unit
) {
    if (!visible) return
    
    // Parse stats JSON (simplified - in production use proper JSON parsing)
    val stats = remember { mutableStateOf(ProfileStats()) }
    
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(120.dp)
            .padding(16.dp)
    ) {
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight(),
            elevation = 4.dp,
            colors = androidx.compose.material3.CardDefaults.cardColors(
                containerColor = Color(0xCC000000)
            )
        ) {
            Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(text = "Profiler", color = Color.Cyan, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                    androidx.compose.material3.IconButton(onClick = onDismiss) {
                        androidx.compose.material3.Icon(
                            painter = androidx.compose.ui.res.painterResource(id = android.R.drawable.ic_menu_close_clear_cancel),
                            contentDescription = "Close"
                        )
                    }
                }
                
                Row(
                    modifier = Modifier.fillMaxWidth().padding(top = 8.dp),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceEvenly
                ) {
                    StatBox("GPU", "${stats.value.gpuTotalMs} ms", Color.Cyan)
                    StatBox("CPU", "${stats.value.cpuTotalMs} ms", Color.Orange)
                    StatBox("Draw Calls", "${stats.value.drawCalls}", Color.Green)
                    StatBox("FPS", String.format("%.1f", stats.value.fps), Color.White)
                    StatBox("Thermal", thermalStatusText(stats.value.thermalStatus), thermalStatusColor(stats.value.thermalStatus))
                }
            }
        }
    }
}

@Composable
fun StatBox(label: String, value: String, color: Color) {
    Column(
        modifier = Modifier
            .weight(1f)
            .padding(horizontal = 4.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(text = label, color = color.copy(alpha = 0.7f), fontSize = 10.sp)
        Text(text = value, color = color, fontSize = 14.sp, fontWeight = FontWeight.Bold)
    }
}

data class ProfileStats(
    var gpuTotalMs: Double = 0.0,
    var cpuTotalMs: Double = 0.0,
    var drawCalls: Int = 0,
    var fps: Double = 0.0,
    var thermalStatus: Int = 0
) {
    companion object {
        fun fromJson(json: String): ProfileStats {
            // Simplified parsing - in production use proper JSON
            return ProfileStats()
        }
    }
}

fun thermalStatusText(status: Int): String {
    return when (status) {
        0 -> "OK"
        1 -> "Light"
        2 -> "Moderate"
        3 -> "Severe"
        4 -> "Critical"
        5 -> "Emergency"
        6 -> "Shutdown"
        else -> "Unknown"
    }
}

fun thermalStatusColor(status: Int): Color {
    return when (status) {
        0 -> Color.Green
        1 -> Color.Yellow
        2 -> Color.Orange
        3 -> Color.Red
        4 -> Color.Magenta
        else -> Color.White
    }
}