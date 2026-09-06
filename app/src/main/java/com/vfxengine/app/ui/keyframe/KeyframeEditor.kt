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
import androidx.compose.foundation.layout.weight
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.Divider
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
 * Keyframe curve editor with bezier visualization and custom graph editing
 */
@Composable
fun KeyframeEditor(state: EditorState) {
    val target = state.keyframeEditorTarget
    var showCustomCurve by remember { mutableStateOf(false) }
    var selectedKeyframeIndex by remember { mutableStateOf<Int?>(null) }
    
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
                KeyframeToolbar(state, target, showCustomCurve, { showCustomCurve = !showCustomCurve })
                
                Divider(color = Color.White.copy(alpha = 0.1f))
                
                if (showCustomCurve && selectedKeyframeIndex != null && selectedKeyframeIndex!! < target.track.keyframes.size) {
                    // Custom curve editor for the selected keyframe
                    CustomCurveEditor(state, target, selectedKeyframeIndex!!, onClose = { showCustomCurve = false })
                } else {
                    // Standard curve editor
                    CurveView(state, target, onKeyframeSelect = { index ->
                        selectedKeyframeIndex = index
                        showCustomCurve = true
                    })
                }
            }
        }
    }
}

@Composable
fun KeyframeToolbar(
    state: EditorState, 
    target: EditorState.KeyframeTarget, 
    showCustomCurve: Boolean,
    onToggleCustomCurve: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(48.dp)
            .padding(16.dp)
            .background(Color(0xFF1E1E1E))
    ) {
        Text(text = "${target.nodeId} > ${target.uniformName}", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
        
        androidx.compose.foundation.layout.Box(modifier = Modifier.weight(1f))
        
        // Interpolation type selector for selected keyframe
        target.keyframeEditorTarget?.let { target ->
            val selectedKf = target.track.keyframes.firstOrNull { it.time == state.currentTimeSeconds }
                ?: target.track.keyframes.firstOrNull()
            selectedKf?.let { kf ->
                androidx.compose.material3.TextButton(onClick = { /* Show interpolation menu */ }) {
                    Text(text = "Interp: ${kf.interpolation.name}", color = Color.Cyan, fontSize = 12.sp)
                }
            }
        }
        
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
        
        IconButton(onClick = onToggleCustomCurve) {
            Icon(
                painter = painterResource(id = android.R.drawable.ic_menu_edit),
                contentDescription = if (showCustomCurve) "Hide custom curve" : "Show custom curve",
                tint = if (showCustomCurve) Color.Cyan else Color.White
            )
        }
    }
}

@Composable
fun CurveView(
    state: EditorState, 
    target: EditorState.KeyframeTarget,
    onKeyframeSelect: (Int) -> Unit
) {
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
                    .background(if (kf.interpolation == EditorState.InterpolationType.Custom) Color.Magenta else Color.Cyan)
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
                            },
                            onDragStart = {
                                onKeyframeSelect(index)
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
fun CustomCurveEditor(
    state: EditorState,
    target: EditorState.KeyframeTarget,
    keyframeIndex: Int,
    onClose: () -> Unit
) {
    val kf = target.track.keyframes[keyframeIndex]
    
    Card(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
            .background(Color(0xFF1E1E1E))
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
            ) {
                Text(text = "Custom Curve Editor", color = Color.White, fontSize = 18.sp, fontWeight = FontWeight.Bold)
                IconButton(onClick = onClose) {
                    Icon(painter = painterResource(id = android.R.drawable.ic_menu_close_clear_cancel), contentDescription = "Close")
                }
            }
            
            Divider(color = Color.White.copy(alpha = 0.1f))
            
            // Custom curve control points editor
            // The customCurvePoints array stores control points for a cubic bezier
            // Format: [cp1x, cp1y, cp2x, cp2y] where each is 0-1
            Column(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
            ) {
                Text(text = "Keyframe at ${formatTimecode(kf.time)}: ${kf.value}", color = Color.White.copy(alpha = 0.8f), fontSize = 14.sp)
                
                // Control point 1
                CurveControlPointEditor(
                    label = "Control Point 1 (In)",
                    x = kf.customCurvePoints.getOrElse(0) { 0.25f },
                    y = kf.customCurvePoints.getOrElse(1) { 0.0f },
                    onChange = { x, y ->
                        while (kf.customCurvePoints.size < 4) { kf.customCurvePoints.add(0f) }
                        kf.customCurvePoints[0] = x
                        kf.customCurvePoints[1] = y
                    }
                )
                
                // Control point 2
                CurveControlPointEditor(
                    label = "Control Point 2 (Out)",
                    x = kf.customCurvePoints.getOrElse(2) { 0.75f },
                    y = kf.customCurvePoints.getOrElse(3) { 1.0f },
                    onChange = { x, y ->
                        while (kf.customCurvePoints.size < 4) { kf.customCurvePoints.add(0f) }
                        kf.customCurvePoints[2] = x
                        kf.customCurvePoints[3] = y
                    }
                )
                
                // Preview of custom curve
                CustomCurvePreview(
                    cp1x = kf.customCurvePoints.getOrElse(0) { 0.25f },
                    cp1y = kf.customCurvePoints.getOrElse(1) { 0.0f },
                    cp2x = kf.customCurvePoints.getOrElse(2) { 0.75f },
                    cp2y = kf.customCurvePoints.getOrElse(3) { 1.0f }
                )
                
                // Preset buttons
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
                ) {
                    PresetCurveButton("Linear", 0f, 0f, 1f, 1f, onClick = {
                        while (kf.customCurvePoints.size < 4) { kf.customCurvePoints.add(0f) }
                        kf.customCurvePoints[0] = 0f; kf.customCurvePoints[1] = 0f
                        kf.customCurvePoints[2] = 1f; kf.customCurvePoints[3] = 1f
                    })
                    PresetCurveButton("Ease In", 0.42f, 0f, 1f, 1f, onClick = {
                        while (kf.customCurvePoints.size < 4) { kf.customCurvePoints.add(0f) }
                        kf.customCurvePoints[0] = 0.42f; kf.customCurvePoints[1] = 0f
                        kf.customCurvePoints[2] = 1f; kf.customCurvePoints[3] = 1f
                    })
                    PresetCurveButton("Ease Out", 0f, 0f, 0.58f, 1f, onClick = {
                        while (kf.customCurvePoints.size < 4) { kf.customCurvePoints.add(0f) }
                        kf.customCurvePoints[0] = 0f; kf.customCurvePoints[1] = 0f
                        kf.customCurvePoints[2] = 0.58f; kf.customCurvePoints[3] = 1f
                    })
                    PresetCurveButton("Ease In-Out", 0.42f, 0f, 0.58f, 1f, onClick = {
                        while (kf.customCurvePoints.size < 4) { kf.customCurvePoints.add(0f) }
                        kf.customCurvePoints[0] = 0.42f; kf.customCurvePoints[1] = 0f
                        kf.customCurvePoints[2] = 0.58f; kf.customCurvePoints[3] = 1f
                    })
                }
            }
        }
    }
}

@Composable
fun CurveControlPointEditor(
    label: String,
    x: Float,
    y: Float,
    onChange: (Float, Float) -> Unit
) {
    var currentX by remember { mutableStateOf(x) }
    var currentY by remember { mutableStateOf(y) }
    
    Column(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
        ) {
            Text(text = label, color = Color.White, fontSize = 13.sp)
            Text(text = "X: ${String.format("%.2f", currentX)}  Y: ${String.format("%.2f", currentY)}", color = Color.Cyan, fontSize = 12.sp)
        }
        
        // 2D slider for control point
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(200.dp)
                .background(Color(0xFF0D0D0D))
                .pointerInput(Unit) {
                    detectDragGestures(
                        onDrag = { change, dragAmount ->
                            currentX = (currentX + dragAmount.x / 300f).coerceIn(0f, 1f)
                            currentY = (currentY - dragAmount.y / 200f).coerceIn(0f, 1f)
                            onChange(currentX, currentY)
                        }
                    )
                }
        ) {
            // Grid
            Canvas(modifier = Modifier.fillMaxSize()) {
                val width = size.width
                val height = size.height
                
                // Diagonal line (linear)
                drawLine(
                    color = Color.White.copy(alpha = 0.2f),
                    start = androidx.compose.ui.geometry.Offset(0f, height),
                    end = androidx.compose.ui.geometry.Offset(width, 0f),
                    strokeWidth = 1f
                )
                
                // Control point visualization
                val cx = currentX * width
                val cy = (1f - currentY) * height
                
                // In/Out handles
                drawLine(
                    color = Color.Green,
                    start = androidx.compose.ui.geometry.Offset(cx, cy),
                    end = androidx.compose.ui.geometry.Offset(0f, height),
                    strokeWidth = 1f
                )
                drawLine(
                    color = Color.Blue,
                    start = androidx.compose.ui.geometry.Offset(cx, cy),
                    end = androidx.compose.ui.geometry.Offset(width, 0f),
                    strokeWidth = 1f
                )
            }
            
            // Control point marker
            Box(
                modifier = Modifier
                    .size(16.dp)
                    .background(Color.Magenta)
                    .graphicsLayer {
                        translationX = (currentX * 300f - 8).px
                        translationY = ((1f - currentY) * 200f - 8).px
                    }
            )
        }
        
        // Sliders for precise control
        Row(
            modifier = Modifier.fillMaxWidth().padding(top = 16.dp),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(text = "X (Time)", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
                Slider(
                    modifier = Modifier.fillMaxWidth(),
                    value = currentX,
                    onValueChange = { currentX = it; onChange(currentX, currentY) },
                    colors = androidx.compose.material3.SliderDefaults.colors(thumbColor = Color.Cyan, activeTrackColor = Color.Cyan)
                )
            }
            Column(modifier = Modifier.weight(1f)) {
                Text(text = "Y (Value)", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
                Slider(
                    modifier = Modifier.fillMaxWidth(),
                    value = currentY,
                    onValueChange = { currentY = it; onChange(currentX, currentY) },
                    colors = androidx.compose.material3.SliderDefaults.colors(thumbColor = Color.Magenta, activeTrackColor = Color.Magenta)
                )
            }
        }
    }
}

@Composable
fun CustomCurvePreview(
    cp1x: Float, cp1y: Float,
    cp2x: Float, cp2y: Float
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(150.dp)
            .background(Color(0xFF0D0D0D))
    ) {
        Canvas(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            val width = size.width
            val height = size.height
            
            // Draw curve
            val path = Path()
            var first = true
            for (i in 0..50) {
                val t = i / 50f
                val x = t * width
                val y = height - cubicBezier(t, 0f, 0f, cp1x, cp1y, cp2x, cp2y, 1f, 1f) * height
                
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
            
            // Draw control points
            drawCircle(Color.Green, radius = 6f, center = androidx.compose.ui.geometry.Offset(cp1x * width, (1f - cp1y) * height))
            drawCircle(Color.Blue, radius = 6f, center = androidx.compose.ui.geometry.Offset(cp2x * width, (1f - cp2y) * height))
        }
    }
}

@Composable
fun PresetCurveButton(label: String, cp1x: Float, cp1y: Float, cp2x: Float, cp2y: Float, onClick: () -> Unit) {
    androidx.compose.material3.TextButton(
        onClick = onClick,
        modifier = Modifier.weight(1f).height(36.dp)
    ) {
        Text(text = label, color = Color.White, fontSize = 11.sp)
    }
}

private fun cubicBezier(t: Float, p0x: Float, p0y: Float, p1x: Float, p1y: Float, p2x: Float, p2y: Float, p3x: Float, p3y: Float): Float {
    val u = 1f - t
    val uu = u * u
    val uuu = uu * u
    val tt = t * t
    val ttt = tt * t
    
    val px = uuu * p0x + 3 * uu * t * p1x + 3 * u * tt * p2x + ttt * p3x
    val py = uuu * p0y + 3 * uu * t * p1y + 3 * u * tt * p2y + ttt * p3y
    
    // Return y normalized (assuming x=t for time-based curves)
    return py
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

private fun formatTimecode(seconds: Double): String {
    val totalFrames = (seconds * 30).roundToInt()
    val hours = totalFrames / (30 * 60 * 60)
    val minutes = (totalFrames / (30 * 60)) % 60
    val secs = (totalFrames / 30) % 60
    val frames = totalFrames % 30
    return String.format("%02d:%02d:%02d:%02d", hours, minutes, secs, frames)
}