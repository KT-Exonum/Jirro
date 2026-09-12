package com.vfxengine.app.ui.playback

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.size
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
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.text.font.FontFamily
import kotlin.math.roundToInt
import com.vfxengine.app.ui.common.EditorState

/**
 * Playback controls (play/pause/stop/seek/speed)
 */
@Composable
fun PlaybackControls(state: EditorState) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(80.dp)
            .background(Color(0xFF121212))
            .padding(16.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Transport controls
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Center,
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Jump to start
                IconButton(onClick = { state.seek(0.0) }) {
                    Icon(
                        painter = painterResource(id = android.R.drawable.ic_media_rew),
                        contentDescription = "Jump to start"
                    )
                }
                
                // Play/Pause
                IconButton(
                    onClick = { 
                        state.setPlaying(!state.isPlaying)
                    },
                    modifier = Modifier.size(56.dp)
                ) {
                    Icon(
                        painter = painterResource(
                            id = if (state.isPlaying) android.R.drawable.ic_media_pause else android.R.drawable.ic_media_play
                        ),
                        contentDescription = if (state.isPlaying) "Pause" else "Play"
                    )
                }
                
                // Stop
                IconButton(onClick = { 
                    state.setPlaying(false)
                    state.seek(0.0)
                }) {
                    Icon(
                        painter = painterResource(id = android.R.drawable.ic_media_pause),
                        contentDescription = "Stop"
                    )
                }
                
                // Jump to end
                IconButton(onClick = { state.seek(state.durationSeconds) }) {
                    Icon(
                        painter = painterResource(id = android.R.drawable.ic_media_next),
                        contentDescription = "Jump to end"
                    )
                }
            }
            
            // Time display and seek slider
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 8.dp),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = formatTime(state.currentTimeSeconds),
                    color = Color.White,
                    fontSize = 14.sp,
                    fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                )
                
                // Seek slider
                var sliderPosition by remember { mutableStateOf(0f) }
                Slider(
                    modifier = Modifier
                        .weight(1f)
                        .padding(horizontal = 16.dp),
                    value = sliderPosition,
                    onValueChange = { newPos ->
                        sliderPosition = newPos
                        val time = newPos * state.durationSeconds
                        if (state.isScrubbing) {
                            state.seek(time)
                        }
                    },
                    onValueChangeFinished = {
                        state.seek(sliderPosition * state.durationSeconds)
                        state.isScrubbing = false
                        state.nativeEngine.setScrubbing(false)
                    },
                    colors = androidx.compose.material3.SliderDefaults.colors(
                        thumbColor = Color.Cyan,
                        activeTrackColor = Color.Cyan,
                        inactiveTrackColor = Color.White.copy(alpha = 0.2f)
                    )
                )
                
                Text(
                    text = formatTime(state.durationSeconds),
                    color = Color.White,
                    fontSize = 14.sp,
                    fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                )
            }
            
            // Speed control
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 8.dp),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.End,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "Speed: ${String.format("%.2f", state.playbackSpeed)}x",
                    color = Color.White.copy(alpha = 0.7f),
                    fontSize = 12.sp
                )
                
                Slider(
                    modifier = Modifier.width(150.dp),
                    value = ((state.playbackSpeed + 2.0) / 4.0).toFloat(),
                    onValueChange = { newPos ->
                        val speed = newPos * 4.0 - 2.0
                        state.setPlaybackSpeed(speed.coerceIn(-2.0, 2.0))
                    },
                    colors = androidx.compose.material3.SliderDefaults.colors(
                        thumbColor = Color(0xFFFFA726),
                        activeTrackColor = Color(0xFFFFA726),
                        inactiveTrackColor = Color.White.copy(alpha = 0.2f)
                    )
                )
            }
        }
    }
}

private fun formatTime(seconds: Double): String {
    val totalSeconds = seconds.roundToInt()
    val hours = totalSeconds / 3600
    val minutes = (totalSeconds % 3600) / 60
    val secs = totalSeconds % 60
    val frames = ((seconds - totalSeconds) * 30).roundToInt()
    return if (hours > 0) String.format("%02d:%02d:%02d.%02d", hours, minutes, secs, frames)
    else String.format("%02d:%02d.%02d", minutes, secs, frames)
}