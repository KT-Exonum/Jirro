package com.vfxengine.app.ui.keyframe

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Button
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
import androidx.compose.ui.graphics.Canvas
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.drawPath
import androidx.compose.ui.graphics.drawscope.stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.px
import androidx.compose.ui.unit.sp
import com.vfxengine.app.ui.common.EditorState

/**
 * Keyframe curve editor with bezier visualization
 */
@Composable
fun KeyframeEditor(state: EditorState) {
    val target = state.keyframeEditorTarget
    
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFF121212))
    ) {
        if (target == null) {
            CenteredText("Select a property in the inspector to edit keyframes")
        } else {
            Column(modifier = Modifier.fillMaxSize()) {
                // Toolbar
                KeyframeToolbar(state, target)
                
                Divider(color = Color.White.copy(alpha = 0.1f))
                
                // Curve editor
                CurveView(state, target)
            }
        }
    }
}

@Composable
fun KeyframeToolbar(state: EditorState, target: EditorState.KeyframeTarget) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(48.dp)
            .padding(16.dp)
            .background(Color(0xFF1E1E1E))
    ) {
        Text(text = "${target.nodeId} > ${target.uniformName}", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
        
        androidx.compose.foundation.layout.Box(modifier = Modifier.weight(1f))
        
        IconButton(onClick = { 
            // Add keyframe at current time
            target.track.keyframes.add(EditorState.Keyframe(
                time = state.currentTimeSeconds,
                value = target.track.evaluate(state.currentTimeSeconds)
            ))
            target.track.keyframes.sortBy { it.time }
        }) {
            Icon(
                painter = painterResource(id = android.R.drawable.ic_input_add),
                contentDescription = "Add keyframe"
            )
        }
        
        IconButton(onClick = { 
            // Delete selected keyframe
        }) {
            Icon(
                painter = painterResource(id = android.R.drawable.ic_delete),
                contentDescription = "Delete keyframe"
            )
        }
    }
}

@Composable
fun CurveView(state: EditorState, target: EditorState.KeyframeTarget) {
    val track = target.track
    val graphHeight = 300f
    val timeRange = state.durationSeconds
    val valueRange = 10f // Assume -5 to 5 for now
    
    Box(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
            .background(Color(0xFF0D0D0D))
            .pointerInput(Unit) {
                detectTapGestures(
                    onTap = { offset ->
                        val time = (offset.x / (800f / timeRange)).coerceIn(0.0, timeRange)
                        val value = (1f - offset.y / graphHeight) * valueRange * 2 - valueRange
                        track.keyframes.add(EditorState.Keyframe(time = time, value = value))
                        track.keyframes.sortBy { it.time }
                    }
                )
            }
    ) {
        // Grid
        Canvas(modifier = Modifier.fillMaxSize()) {
            val width = size.width
            val height = size.height
            
            // Horizontal grid lines
            repeat(5) { i ->
                val y = (i / 4f) * height
                drawLine(
                    color = Color.White.copy(alpha = 0.1f),
                    start = androidx.compose.ui.geometry.Offset(0f, y),
                    end = androidx.compose.ui.geometry.Offset(width, y),
                    strokeWidth = 1f
                )
            }
            
            // Vertical grid lines
            repeat(10) { i ->
                val x = (i / 10f) * width
                drawLine(
                    color = Color.White.copy(alpha = 0.1f),
                    start = androidx.compose.ui.geometry.Offset(x, 0f),
                    end = androidx.compose.ui.geometry.Offset(x, height),
                    strokeWidth = 1f
                )
            }
            
            // Zero line
            val zeroY = height / 2
            drawLine(
                color = Color.White.copy(alpha = 0.3f),
                start = androidx.compose.ui.geometry.Offset(0f, zeroY),
                end = androidx.compose.ui.geometry.Offset(width, zeroY),
                strokeWidth = 1f
            )
        }
        
        // Curve path
        if (track.keyframes.size >= 2) {
            Canvas(modifier = Modifier.fillMaxSize()) {
                val width = size.width
                val height = size.height
                val path = Path()
                
                var first = true
                for (i in 0..100) {
                    val t = i / 100f
                    val time = t * timeRange
                    val value = track.evaluate(time)
                    val x = t * width
                    val y = height - (value + valueRange) / (valueRange * 2) * height
                    
                    if (first) {
                        path.moveTo(x, y)
                        first = false
                    } else {
                        path.lineTo(x, y)
                    }
                }
                
                drawPath(
                    path = path,
                    color = Color.Cyan,
                    style = androidx.compose.ui.graphics.Stroke(width = 2f)
                )
            }
        }
        
        // Keyframe points
        track.keyframes.forEachIndexed { index, kf ->
            val x = (kf.time / timeRange) * 800f
            val y = 300f - (kf.value + valueRange) / (valueRange * 2) * 300f
            
            Box(
                modifier = Modifier
                    .size(16.dp)
                    .background(Color.Cyan)
                    .graphicsLayer {
                        translationX = (x - 8).px
                        translationY = (y - 8).px
                    }
                    .pointerInput(kf) {
                        detectDragGestures(
                            onDrag = { change, dragAmount ->
                                val newTime = ((x + dragAmount.x) / 800f * timeRange).coerceIn(0.0, timeRange)
                                val newValue = ((300f - (y + dragAmount.y)) / 300f * valueRange * 2 - valueRange).coerceIn(-valueRange, valueRange)
                                kf.time = newTime
                                kf.value = newValue
                                track.keyframes.sortBy { it.time }
                            }
                        )
                    }
            )
        }
        
        // Current time indicator
        val playheadX = (state.currentTimeSeconds / timeRange) * 800f
        Box(
            modifier = Modifier
                .width(2.dp)
                .fillMaxHeight()
                .background(Color.Red)
                .graphicsLayer { translationX = playheadX.px }
        )
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