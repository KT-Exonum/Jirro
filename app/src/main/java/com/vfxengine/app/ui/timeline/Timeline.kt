package com.vfxengine.app.ui.timeline

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.GraphicsLayerScope
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.Layout
import androidx.compose.ui.layout.Measurable
import androidx.compose.ui.layout.MeasureResult
import androidx.compose.ui.layout.Placeable
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.px
import androidx.compose.ui.unit.sp
import com.vfxengine.app.ui.common.EditorState

/**
 * Timeline ruler showing time markers
 */
@Composable
fun TimelineRuler(state: EditorState, rulerHeight: Float = 30f) {
    val totalWidth = state.durationSeconds * state.timeScale
    val visibleStart = (-state.timeOffset / state.timeScale).coerceAtLeast(0.0)
    val visibleEnd = visibleStart + (800f / state.timeScale) // approximate visible width

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(rulerHeight.dp)
            .background(Color(0xFF1E1E1E))
            .clipToBounds()
    ) {
        // Time markers
        val markerInterval = when {
            state.timeScale > 200 -> 0.5
            state.timeScale > 100 -> 1.0
            state.timeScale > 50 -> 2.0
            state.timeScale > 20 -> 5.0
            else -> 10.0
        }

        var markerTime = (visibleStart / markerInterval).ceil() * markerInterval
        while (markerTime <= visibleEnd && markerTime <= state.durationSeconds) {
            val x = markerTime * state.timeScale + state.timeOffset
            if (x >= 0 && x <= 800) {
                Box(
                    modifier = Modifier
                        .width(1.dp)
                        .fillMaxHeight()
                        .background(Color.White.copy(alpha = 0.3f))
                        .graphicsLayer { translationX = x.px }
                )
                // Time label
                Text(
                    text = formatTime(markerTime),
                    color = Color.White.copy(alpha = 0.7f),
                    fontSize = 9.sp,
                    modifier = Modifier
                        .padding(top = 2.dp, start = 2.dp)
                        .graphicsLayer { translationX = (x + 2).px }
                )
            }
            markerTime += markerInterval
        }
    }
}

/**
 * Timeline track showing clips
 */
@Composable
fun TimelineTrack(
    state: EditorState,
    trackHeight: Float = 60f,
    layer: Int
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(trackHeight.dp)
            .background(if (layer % 2 == 0) Color(0xFF252525) else Color(0xFF1E1E1E))
            .clipToBounds()
            .pointerInput(Unit) {
                detectTapGestures(
                    onTap = { offset ->
                        val time = (offset.x - state.timeOffset) / state.timeScale
                        state.seek(time.coerceIn(0.0, state.durationSeconds))
                    },
                    onLongPress = { offset ->
                        // Could show context menu
                    }
                )
                detectDragGestures(
                    onDragStart = { },
                    onDrag = { change, dragAmount ->
                        if (!state.isScrubbing) {
                            state.isScrubbing = true
                            state.nativeEngine.setScrubbing(true)
                            state.isPlaying = false
                            state.nativeEngine.setPlaying(false)
                        }
                        val time = (change.position.x - state.timeOffset) / state.timeScale
                        state.seek(time.coerceIn(0.0, state.durationSeconds))
                    },
                    onDragEnd = { }
                )
            }
    ) {
        // Clips on this layer
        state.clips.value
            .filter { it.layer == layer }
            .forEach { clip ->
                val clipStartX = clip.timelineStart * state.timeScale + state.timeOffset
                val clipWidth = (clip.sourceOut - clip.sourceIn) / Math.abs(clip.speed) * state.timeScale
                
                Box(
                    modifier = Modifier
                        .width(clipWidth.px)
                        .height((trackHeight - 8).dp)
                        .background(Color(clip.color))
                        .graphicsLayer {
                            translationX = clipStartX.px
                            translationY = 4.px
                        }
                        .pointerInput(clip) {
                            detectDragGestures(
                                onDrag = { change, dragAmount ->
                                    val deltaTime = dragAmount.x / state.timeScale
                                    clip.timelineStart = (clip.timelineStart + deltaTime).coerceIn(0.0, state.durationSeconds - clipWidth / state.timeScale)
                                    // Send clip position update to native
                                }
                            )
                        }
                ) {
                    // Clip label
                    Text(
                        text = clip.name,
                        color = Color.White,
                        fontSize = 10.sp,
                        modifier = Modifier.padding(4.dp)
                    )
                }
            }
    }
}

/**
 * Playhead overlay
 */
@Composable
fun PlayheadOverlay(state: EditorState, trackHeight: Float, layerCount: Int) {
    val playheadX = state.currentTimeSeconds * state.timeScale + state.timeOffset
    
    Box(
        modifier = Modifier
            .width(2.dp)
            .height((trackHeight * layerCount + 30).dp)
            .background(Color.Red)
            .graphicsLayer { translationX = playheadX.px }
    )
    
    // Time display at top
    Text(
        text = formatTime(state.currentTimeSeconds),
        color = Color.Red,
        fontSize = 12.sp,
        fontWeight = FontWeight.Bold,
        modifier = Modifier
            .padding(top = 4.dp, start = 4.dp)
            .graphicsLayer { translationX = (playheadX + 4).px }
    )
}

/**
 * Main timeline component
 */
@Composable
fun Timeline(state: EditorState) {
    val layerCount = if (state.clips.value.isEmpty()) 1 else state.clips.value.maxByOrNull { it.layer }?.layer?.plus(1) ?: 1
    val trackHeight = 60f
    val rulerHeight = 30f

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .height((rulerHeight + trackHeight * layerCount).dp)
            .background(Color(0xFF121212))
            .pointerInput(Unit) {
                detectDragGestures(
                    onDrag = { change, dragAmount ->
                        // Pan timeline
                        state.timeOffset += dragAmount.x
                    }
                )
                detectTapGestures(
                    onTap = { offset ->
                        val time = (offset.x - state.timeOffset) / state.timeScale
                        state.seek(time.coerceIn(0.0, state.durationSeconds))
                    }
                )
            }
    ) {
        TimelineRuler(state, rulerHeight)
        
        // Tracks
        repeat(layerCount) { layer ->
            TimelineTrack(state, trackHeight, layer)
        }
        
        // Playhead
        PlayheadOverlay(state, trackHeight, layerCount)
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