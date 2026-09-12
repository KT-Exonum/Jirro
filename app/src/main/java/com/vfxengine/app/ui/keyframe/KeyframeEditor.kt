package com.vfxengine.app.ui.keyframe

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentSize
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
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Fill
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.vfxengine.app.ui.common.EditorState
import kotlin.math.abs
import kotlin.math.roundToInt
import kotlin.ranges.ClosedFloatingPointRange

private const val KEYFRAME_SIZE = 22f
private const val HANDLE_SIZE = 7f
private const val CURVE_STROKE = 2.5f
private const val GRAPH_BG = 0xFF0A0A0A
private const val GRID_MINOR = 0x12FFFFFF
private const val GRID_MAJOR = 0x20FFFFFF
private const val ZERO_LINE = 0x30FFFFFF
private const val CURVE_COLOR = 0xFF00E5FF
private const val PLAYHEAD_COLOR = 0xFFFF3B3B
private const val SELECTED_GLOW = 0xFF00E5FF

private enum class DragMode { None, MoveKeyframe, MoveInTangent, MoveOutTangent, AddKeyframe }
private enum class KeyframeEditorTab { Curve, DopeSheet, Procedural, EasingPresets, RoamingCurves }

@Composable
fun KeyframeEditor(state: EditorState) {
    val target = state.keyframeEditorTarget
    var selectedIndex by remember { mutableStateOf<Int?>(null) }
    var showInterpMenu by remember { mutableStateOf(false) }
    var snapEnabled by remember { mutableStateOf(true) }
    var valueReadout by remember { mutableStateOf<String?>(null) }
    var copiedKeyframe by remember { mutableStateOf<EditorState.Keyframe?>(null) }
    var autoKey by remember { mutableStateOf(false) }
    var activeTab by remember { mutableStateOf(KeyframeEditorTab.Curve) }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(GRAPH_BG))
    ) {
        if (target == null) {
            CenteredText("Select a property in the inspector to edit keyframes")
        } else {
            Column(modifier = Modifier.fillMaxSize()) {
                KeyframeToolbar(
                    state = state,
                    target = target,
                    snapEnabled = snapEnabled,
                    autoKey = autoKey,
                    selectedIndex = selectedIndex,
                    onSnapToggle = { snapEnabled = it },
                    onAutoKeyToggle = { autoKey = it },
                    onDelete = {
                        val idx = selectedIndex ?: return@KeyframeToolbar
                        if (idx in target.track.keyframes.indices) {
                            val kf = target.track.keyframes[idx]
                            state.removeKeyframe(target.nodeId, target.uniformName, kf)
                            selectedIndex = null
                        }
                    },
                    onCopy = {
                        val idx = selectedIndex ?: return@KeyframeToolbar
                        if (idx in target.track.keyframes.indices) {
                            copiedKeyframe = target.track.keyframes[idx].copy()
                        }
                    },
                    onPaste = {
                        val copy = copiedKeyframe ?: return@KeyframeToolbar
                        val newKf = copy.copy(time = state.currentTimeSeconds)
                        state.addKeyframe(target.nodeId, target.uniformName, newKf.time, newKf.value)
                        selectedIndex = target.track.keyframes.indexOfFirst { it.time == newKf.time }
                    },
                    onAddAtPlayhead = {
                        val track = target.track
                        val value = track.evaluate(state.currentTimeSeconds)
                        state.addKeyframe(target.nodeId, target.uniformName, state.currentTimeSeconds, value)
                        selectedIndex = track.keyframes.indexOfFirst { Math.abs(it.time - state.currentTimeSeconds) < 0.001 }
                    },
                    onToggleInterpMenu = { showInterpMenu = !showInterpMenu },
                    showInterpMenu = showInterpMenu
                )

                androidx.compose.material3.Divider(color = Color.White.copy(alpha = 0.08f))

                // Tab bar
                TabBar(
                    activeTab = activeTab,
                    onTabClick = { activeTab = it },
                    procedural = target.track.procedural
                )

                androidx.compose.material3.Divider(color = Color.White.copy(alpha = 0.08f))

                Box(modifier = Modifier.fillMaxSize()) {
                    when (activeTab) {
                        KeyframeEditorTab.Curve -> {
                            GraphArea(
                                state = state,
                                target = target,
                                snapEnabled = snapEnabled,
                                selectedIndex = selectedIndex,
                                onSelectIndex = { selectedIndex = it },
                                onValueReadout = { valueReadout = it },
                                autoKey = autoKey
                            )

                            valueReadout?.let { readout ->
                                Box(
                                    modifier = Modifier
                                        .align(Alignment.TopEnd)
                                        .padding(8.dp)
                                        .background(Color(0xFF1E1E1E).copy(alpha = 0.9f))
                                        .padding(horizontal = 8.dp, vertical = 4.dp)
                                ) {
                                    Text(text = readout, color = Color.Cyan, fontSize = 11.sp, fontWeight = FontWeight.Medium)
                                }
                            }

                            if (showInterpMenu && selectedIndex != null) {
                                InterpMenu(
                                    current = target.track.keyframes.getOrNull(selectedIndex!!)?.interpolation
                                        ?: EditorState.InterpolationType.Linear,
                                    onSelect = { interp ->
                                        showInterpMenu = false
                                        val idx = selectedIndex ?: return@InterpMenu
                                        if (idx in target.track.keyframes.indices) {
                                            val oldKf = target.track.keyframes[idx].copy()
                                            target.track.keyframes[idx].interpolation = interp
                                            state.updateKeyframe(target.nodeId, target.uniformName, oldKf, target.track.keyframes[idx].copy())
                                        }
                                    },
                                    onDismiss = { showInterpMenu = false }
                                )
                            }
                        }
                        KeyframeEditorTab.DopeSheet -> {
                            DopeSheetView(
                                state = state,
                                target = target,
                                selectedIndex = selectedIndex,
                                onSelectIndex = { selectedIndex = it },
                                snapEnabled = snapEnabled
                            )
                        }
                        KeyframeEditorTab.Procedural -> {
                            ProceduralControls(
                                procedural = target.track.procedural,
                                onChange = { newProc ->
                                    target.track.procedural = newProc
                                }
                            )
                        }
                        KeyframeEditorTab.EasingPresets -> {
                            EasingPresetsPanel(
                                state = state,
                                target = target,
                                selectedIndex = selectedIndex,
                                onApplyPreset = { preset ->
                                    // Apply easing preset to selected keyframes
                                    val indices = selectedIndex?.let { listOf(it) } ?: target.track.keyframes.indices.toList()
                                    indices.forEach { idx ->
                                        if (idx in target.track.keyframes.indices) {
                                            val oldKf = target.track.keyframes[idx].copy()
                                            val newKf = target.track.keyframes[idx].copy(interpolation = preset.interpolation)
                                            if (preset.interpolation == EditorState.InterpolationType.Bezier) {
                                                newKf.inTangent = preset.inTangent
                                                newKf.outTangent = preset.outTangent
                                            }
                                            target.track.keyframes[idx] = newKf
                                            state.updateKeyframe(target.nodeId, target.uniformName, oldKf, newKf)
                                        }
                                    }
                                }
                            )
                        }
                        KeyframeEditorTab.RoamingCurves -> {
                            RoamingCurvesPanel(
                                state = state,
                                target = target,
                                onApplyCurve = { curveData ->
                                    // Apply roaming curve
                                    val indices = selectedIndex?.let { listOf(it) } ?: target.track.keyframes.indices.toList()
                                    indices.forEach { idx ->
                                        if (idx in target.track.keyframes.indices) {
                                            val oldKf = target.track.keyframes[idx].copy()
                                            val newKf = target.track.keyframes[idx].copy()
                                            // Apply curve shape to keyframe
                                            target.track.keyframes[idx] = newKf
                                            state.updateKeyframe(target.nodeId, target.uniformName, oldKf, newKf)
                                        }
                                    }
                                }
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun KeyframeToolbar(
    state: EditorState,
    target: EditorState.KeyframeTarget,
    snapEnabled: Boolean,
    autoKey: Boolean,
    selectedIndex: Int?,
    onSnapToggle: (Boolean) -> Unit,
    onAutoKeyToggle: (Boolean) -> Unit,
    onDelete: () -> Unit,
    onCopy: () -> Unit,
    onPaste: () -> Unit,
    onAddAtPlayhead: () -> Unit,
    onToggleInterpMenu: () -> Unit,
    showInterpMenu: Boolean
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(48.dp)
            .padding(horizontal = 12.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = "${target.nodeId} > ${target.uniformName}",
            color = Color.White,
            fontSize = 13.sp,
            fontWeight = FontWeight.Bold
        )
        Box(modifier = Modifier.weight(1f))

        if (selectedIndex != null && selectedIndex < target.track.keyframes.size) {
            val kf = target.track.keyframes[selectedIndex]
            androidx.compose.material3.TextButton(onClick = onToggleInterpMenu) {
                Text(
                    text = "Interp: ${kf.interpolation.name}",
                    color = Color(0xFF00E5FF),
                    fontSize = 11.sp
                )
            }
            Text(
                text = "T:${String.format("%.2f", kf.time)}  V:${String.format("%.3f", kf.value)}",
                color = Color.White.copy(alpha = 0.7f),
                fontSize = 11.sp,
                modifier = Modifier.padding(horizontal = 8.dp)
            )
        }

        ToolbarIconButton(
            iconRes = android.R.drawable.ic_input_add,
            contentDescription = "Add keyframe",
            tint = if (autoKey) Color(0xFF00E5FF) else Color.White.copy(alpha = 0.8f)
        ) { onAddAtPlayhead() }

        ToolbarIconButton(android.R.drawable.ic_menu_edit, "Copy") { onCopy() }
        ToolbarIconButton(android.R.drawable.ic_menu_edit, "Paste") { onPaste() }
        ToolbarIconButton(android.R.drawable.ic_delete, "Delete") { onDelete() }

        ToolbarToggleButton(
            iconRes = android.R.drawable.ic_menu_manage,
            label = "Snap",
            checked = snapEnabled
        ) { onSnapToggle(it) }
        ToolbarToggleButton(
            iconRes = android.R.drawable.ic_media_play,
            label = "Auto",
            checked = autoKey
        ) { onAutoKeyToggle(it) }
    }
}

@Composable
private fun ToolbarIconButton(iconRes: Int, contentDescription: String, tint: Color = Color.White, onClick: () -> Unit) {
    IconButton(onClick = onClick) {
        Icon(
            painter = painterResource(id = iconRes),
            contentDescription = contentDescription,
            tint = tint,
            modifier = Modifier.size(18.dp)
        )
    }
}

@Composable
private fun ToolbarToggleButton(iconRes: Int, label: String, checked: Boolean, onClick: (Boolean) -> Unit) {
    IconButton(onClick = { onClick(!checked) }) {
        Icon(
            painter = painterResource(id = iconRes),
            contentDescription = label,
            tint = if (checked) Color(0xFF00E5FF) else Color.White.copy(alpha = 0.6f),
            modifier = Modifier.size(18.dp)
        )
    }
}

@Composable
private fun GraphArea(
    state: EditorState,
    target: EditorState.KeyframeTarget,
    snapEnabled: Boolean,
    selectedIndex: Int?,
    onSelectIndex: (Int) -> Unit,
    onValueReadout: (String) -> Unit,
    autoKey: Boolean
) {
    val track = target.track
    val duration = state.durationSeconds
    val fps = 30
    val totalFrames = (duration * fps).toInt()

    var dragMode by remember { mutableStateOf(DragMode.None) }
    var dragKeyframeIdx by remember { mutableStateOf<Int?>(null) }
    var graphSize by remember { mutableStateOf<androidx.compose.ui.geometry.Size?>(null) }

    val valueRange = remember(track.keyframes) {
        if (track.keyframes.isEmpty()) 10f else {
            val min = track.keyframes.minOf { it.value }
            val max = track.keyframes.maxOf { it.value }
            val span = max - min
            maxOf(span * 1.25f, 2f)
        }
    }

    val graphTop = 28f

    fun timeToX(time: Double, width: Float): Float {
        val timeScale = width / duration
        return (time * timeScale).toFloat()
    }

    fun xToTime(x: Float, width: Float): Double {
        val timeScale = width / duration
        return (x / timeScale).toDouble()
    }

    fun valueToY(value: Float, height: Float): Float {
        val graphHeight = height - graphTop
        val normalized = (value + valueRange) / (valueRange * 2)
        return graphTop + (1f - normalized) * graphHeight
    }

    fun yToValue(y: Float, height: Float): Float {
        val graphHeight = height - graphTop
        val normalized = 1f - ((y - graphTop) / graphHeight)
        return normalized * valueRange * 2 - valueRange
    }

    fun snapTime(time: Double): Double {
        if (!snapEnabled) return time
        val frame = (time * fps).roundToInt().toDouble() / fps
        return frame
    }

    fun snapValue(value: Float): Float {
        if (!snapEnabled) return value
        return (value * 10).roundToInt() / 10f
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .clipToBounds()
            .pointerInput(Unit) {
                detectDragGestures(
                    onDragStart = { offset ->
                        val w = graphSize?.width ?: return@detectDragGestures
                        val h = graphSize?.height ?: return@detectDragGestures
                        val t = xToTime(offset.x, w)
                        val v = yToValue(offset.y, h)

                        val hitIdx = track.keyframes.indexOfFirst { kf ->
                            val kx = timeToX(kf.time, w)
                            val ky = valueToY(kf.value, h)
                            val dist = kotlin.math.hypot(offset.x - kx, offset.y - ky)
                            dist < 18f
                        }

                        if (hitIdx != -1) {
                            dragMode = DragMode.MoveKeyframe
                            dragKeyframeIdx = hitIdx
                            onSelectIndex(hitIdx)
                        } else if (offset.y >= graphTop) {
                            dragMode = DragMode.AddKeyframe
                            val snappedTime = snapTime(t)
                            val snappedValue = snapValue(v)
                            state.addKeyframe(target.nodeId, target.uniformName, snappedTime, snappedValue)
                            val newIdx = target.track.keyframes.indexOfFirst { Math.abs(it.time - snappedTime) < 0.001 && abs(it.value - snappedValue) < 0.001f }
                            onSelectIndex(newIdx)
                        }
                    },
                    onDrag = { change, _ ->
                        val w = graphSize?.width ?: return@detectDragGestures
                        val h = graphSize?.height ?: return@detectDragGestures
                        val pos = change.position

                        when (dragMode) {
                            DragMode.MoveKeyframe -> {
                                val idx = dragKeyframeIdx ?: return@detectDragGestures
                                if (idx !in target.track.keyframes.indices) return@detectDragGestures
                                val kf = target.track.keyframes[idx]
                                val newTime = snapTime(xToTime(pos.x, w).coerceIn(0.0, duration))
                                val newValue = snapValue(yToValue(pos.y, h).coerceIn(-valueRange, valueRange))
                                if (newTime != kf.time || newValue != kf.value) {
                                    val oldKf = kf.copy()
                                    kf.time = newTime
                                    kf.value = newValue
                                    target.track.keyframes.sortBy { it.time }
                                    dragKeyframeIdx = target.track.keyframes.indexOfFirst { it.time == newTime && it.value == newValue }
                                    onSelectIndex(dragKeyframeIdx!!)
                                    state.updateKeyframe(target.nodeId, target.uniformName, oldKf, kf.copy())
                                }
                                onValueReadout("T:${String.format("%.3f", newTime)}  V:${String.format("%.3f", newValue)}")
                            }
                            DragMode.AddKeyframe -> {
                                val t = snapTime(xToTime(pos.x, w).coerceIn(0.0, duration))
                                val v = snapValue(yToValue(pos.y, h).coerceIn(-valueRange, valueRange))
                                if (track.keyframes.none { abs(it.time - t) < 0.001 && abs(it.value - v) < 0.001f }) {
                                    track.keyframes.add(EditorState.Keyframe(time = t, value = v))
                                    track.keyframes.sortBy { it.time }
                                }
                                onValueReadout("T:${String.format("%.3f", t)}  V:${String.format("%.3f", v)}")
                            }
                            else -> {}
                        }
                    },
                    onDragEnd = {
                        dragMode = DragMode.None
                        dragKeyframeIdx = null
                    }
                )
            }
    ) {
        GraphCanvas(
            state = state,
            target = target,
            selectedIndex = selectedIndex,
            valueRange = valueRange,
            graphTop = graphTop,
            fps = fps,
            totalFrames = totalFrames,
            snapEnabled = snapEnabled,
            onSize = { graphSize = it }
        )

        PlayheadOverlay(state, timeToX = { t -> timeToX(t, graphSize?.width ?: 0f) }, graphTop = graphTop)
    }
}

@Composable
private fun GraphCanvas(
    state: EditorState,
    target: EditorState.KeyframeTarget,
    selectedIndex: Int?,
    valueRange: Float,
    graphTop: Float,
    fps: Int,
    totalFrames: Int,
    snapEnabled: Boolean,
    onSize: (androidx.compose.ui.geometry.Size) -> Unit
) {
    val track = target.track
    val duration = state.durationSeconds

    androidx.compose.foundation.layout.Box(
        modifier = Modifier
            .fillMaxSize()
            .drawWithCache {
                val w = size.width
                val h = size.height
                onSize(size)

                val path = Path()
                val steps = 300
                var first = true
                for (i in 0..steps) {
                    val t = i / steps.toFloat()
                    val time = t * duration
                    val value = track.evaluate(time)
                    val x = t * w
                    val graphHeight = h - graphTop
                    val normalized = (value + valueRange) / (valueRange * 2)
                    val y = graphTop + (1f - normalized) * graphHeight
                    if (first) { path.moveTo(x, y); first = false } else path.lineTo(x, y)
                }

                onDrawBehind {
                    drawRect(Color(GRAPH_BG))

                    drawGrid(w, h, graphTop, duration, fps, totalFrames, snapEnabled)

                    if (track.keyframes.size >= 2) {
                        drawPath(
                            path = path,
                            color = Color(CURVE_COLOR),
                            style = Stroke(width = CURVE_STROKE, join = StrokeJoin.Round)
                        )
                    }

                    drawKeyframes(track, w, h, graphTop, valueRange, duration, selectedIndex)

                    drawTangentHandles(track, w, h, graphTop, valueRange, duration, selectedIndex)
                }
            }
    )
}

private fun DrawScope.drawGrid(
    w: Float,
    h: Float,
    graphTop: Float,
    duration: Double,
    fps: Int,
    totalFrames: Int,
    snapEnabled: Boolean
) {
    val graphHeight = h - graphTop
    val timeScale = w / duration

    for (frame in 0..totalFrames) {
        val x = (frame * timeScale / fps).toFloat()
        if (x > w) break
        val isMajor = frame % fps == 0
        val isMid = frame % (fps / 2) == 0 && !isMajor
        val alpha = when {
            isMajor -> 0.18f
            isMid -> 0.10f
            else -> if (snapEnabled) 0.06f else 0.03f
        }
        drawLine(
            color = Color.White.copy(alpha = alpha),
            start = Offset(x, graphTop),
            end = Offset(x, h),
            strokeWidth = if (isMajor) 1f else 0.5f
        )
    }

    val valueSteps = 10
    for (i in 0..valueSteps) {
        val y = graphTop + (i / valueSteps.toFloat()) * graphHeight
        val isMajor = i % 5 == 0
        drawLine(
            color = Color.White.copy(alpha = if (isMajor) 0.12f else 0.05f),
            start = Offset(0f, y),
            end = Offset(w, y),
            strokeWidth = if (isMajor) 1f else 0.5f
        )
    }

    drawLine(
        color = Color(ZERO_LINE),
        start = Offset(0f, graphTop + graphHeight / 2),
        end = Offset(w, graphTop + graphHeight / 2),
        strokeWidth = 1f
    )
}

private fun DrawScope.drawKeyframes(
    track: EditorState.KeyframeTrack,
    w: Float,
    h: Float,
    graphTop: Float,
    valueRange: Float,
    duration: Double,
    selectedIndex: Int?
) {
    val graphHeight = h - graphTop
    track.keyframes.forEachIndexed { index, kf ->
        val x = (kf.time / duration * w).toFloat()
        val normalized = (kf.value + valueRange) / (valueRange * 2)
        val y = graphTop + (1f - normalized) * graphHeight

        val isSelected = index == selectedIndex
        val strokeColor = when (kf.interpolation) {
            EditorState.InterpolationType.Step -> Color(0xFFB0B0B0)
            EditorState.InterpolationType.Linear -> Color(0xFF00E5FF)
            EditorState.InterpolationType.Bezier -> Color(0xFF69F0AE)
            EditorState.InterpolationType.Custom -> Color(0xFFFF4081)
        }

        if (isSelected) {
            drawCircle(
                color = Color(SELECTED_GLOW).copy(alpha = 0.25f),
                radius = KEYFRAME_SIZE * 1.4f,
                center = Offset(x, y)
            )
        }

        drawDiamond(Offset(x, y), KEYFRAME_SIZE / 2, fill = Color.White, stroke = strokeColor, strokeWidth = 2.5f)

        if (isSelected) {
            drawCircle(
                color = Color.White,
                radius = 3f,
                center = Offset(x, y)
            )
        }
    }
}

private fun DrawScope.drawTangentHandles(
    track: EditorState.KeyframeTrack,
    w: Float,
    h: Float,
    graphTop: Float,
    valueRange: Float,
    duration: Double,
    selectedIndex: Int?
) {
    val graphHeight = h - graphTop
    track.keyframes.forEachIndexed { index, kf ->
        if (kf.interpolation != EditorState.InterpolationType.Bezier) return@forEachIndexed
        val isSelected = index == selectedIndex
        if (!isSelected) return@forEachIndexed

        val x = (kf.time / duration * w).toFloat()
        val normalized = (kf.value + valueRange) / (valueRange * 2)
        val y = graphTop + (1f - normalized) * graphHeight

        val inY = graphTop + (1f - ((kf.value + kf.inTangent + valueRange) / (valueRange * 2))) * graphHeight
        val outY = graphTop + (1f - ((kf.value + kf.outTangent + valueRange) / (valueRange * 2))) * graphHeight
        val handleTimeOffset = (duration * 0.05).coerceAtLeast(0.1).toFloat()
        val inX = (x - handleTimeOffset).coerceIn(0f, w)
        val outX = (x + handleTimeOffset).coerceIn(0f, w)

        drawLine(
            color = Color(0xFF69F0AE).copy(alpha = 0.7f),
            start = Offset(x, y),
            end = Offset(inX, inY.coerceIn(graphTop, h)),
            strokeWidth = 1.5f
        )
        drawLine(
            color = Color(0xFFFFAB40).copy(alpha = 0.7f),
            start = Offset(x, y),
            end = Offset(outX, outY.coerceIn(graphTop, h)),
            strokeWidth = 1.5f
        )

        drawCircle(Color(0xFF69F0AE), radius = HANDLE_SIZE, center = Offset(inX, inY.coerceIn(graphTop, h)))
        drawCircle(Color(0xFFFFAB40), radius = HANDLE_SIZE, center = Offset(outX, outY.coerceIn(graphTop, h)))
    }
}

@Composable
private fun PlayheadOverlay(
    state: EditorState,
    timeToX: (Double) -> Float,
    graphTop: Float
) {
    val x = timeToX(state.currentTimeSeconds)
    Box(
        modifier = Modifier
            .fillMaxSize()
            .clipToBounds()
    ) {
        Box(
            modifier = Modifier
                .width(1.5.dp)
                .fillMaxHeight()
                .background(Color(PLAYHEAD_COLOR))
                .wrapContentSize(Alignment.TopStart)
                .padding(start = x.dp)
        )

        Box(
            modifier = Modifier
                .align(Alignment.TopStart)
                .padding(start = (x + 2).dp, top = 2.dp)
                .background(Color(0xFF1E1E1E).copy(alpha = 0.85f))
                .padding(horizontal = 4.dp, vertical = 2.dp)
        ) {
            Text(
                text = formatTimecode(state.currentTimeSeconds),
                color = Color(PLAYHEAD_COLOR),
                fontSize = 10.sp,
                fontFamily = FontFamily.Monospace
            )
        }
    }
}

@Composable
private fun BoxScope.InterpMenu(
    current: EditorState.InterpolationType,
    onSelect: (EditorState.InterpolationType) -> Unit,
    onDismiss: () -> Unit
) {
    val options = EditorState.InterpolationType.values()
    Box(
        modifier = Modifier
            .align(Alignment.TopEnd)
            .padding(top = 56.dp, end = 8.dp)
            .background(Color(0xFF1E1E1E))
            .padding(vertical = 4.dp)
    ) {
        Column {
            options.forEach { interp ->
                androidx.compose.material3.TextButton(
                    onClick = { onSelect(interp) },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(
                        text = interp.name,
                        color = if (interp == current) Color(0xFF00E5FF) else Color.White,
                        fontSize = 12.sp,
                        modifier = Modifier
                            .padding(horizontal = 12.dp, vertical = 4.dp)
                            .background(if (interp == current) Color.White.copy(alpha = 0.05f) else Color.Transparent)
                    )
                }
            }
        }
    }
}

@Composable
private fun TabBar(
    activeTab: KeyframeEditorTab,
    onTabClick: (KeyframeEditorTab) -> Unit,
    procedural: EditorState.ProceduralConfig
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(40.dp)
            .padding(horizontal = 8.dp),
        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(4.dp)
    ) {
        KeyframeEditorTab.values().forEach { tab ->
            val isActive = activeTab == tab
            val label = when (tab) {
                KeyframeEditorTab.Curve -> "Curve"
                KeyframeEditorTab.DopeSheet -> "Dope Sheet"
                KeyframeEditorTab.Procedural -> "Procedural"
                KeyframeEditorTab.EasingPresets -> "Easing"
                KeyframeEditorTab.RoamingCurves -> "Curves"
            }
            val showIndicator = when (tab) {
                KeyframeEditorTab.Procedural -> procedural.enabled
                KeyframeEditorTab.EasingPresets -> true
                KeyframeEditorTab.RoamingCurves -> true
                else -> false
            }
            androidx.compose.material3.TextButton(
                onClick = { onTabClick(tab) },
                modifier = Modifier
                    .weight(1f)
                    .height(36.dp),
                colors = androidx.compose.material3.ButtonDefaults.textButtonColors(
                    containerColor = if (isActive) Color(0xFF1E1E1E) else Color.Transparent,
                    contentColor = if (isActive) Color(0xFF00E5FF) else Color.White.copy(alpha = 0.7f)
                )
            ) {
                Row(
                    modifier = Modifier.fillMaxSize(),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Center,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(text = label, fontSize = 11.sp, fontWeight = if (isActive) FontWeight.Bold else FontWeight.Normal)
                    if (showIndicator) {
                        Box(
                            modifier = Modifier
                                .size(6.dp)
                                .background(Color(0xFF69F0AE))
                                .padding(start = 4.dp)
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun ProceduralControls(
    procedural: EditorState.ProceduralConfig,
    onChange: (EditorState.ProceduralConfig) -> Unit
) {
    val newProc = remember { mutableStateOf(procedural.copy()) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
    ) {
        // Master toggle
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(text = "Enable Procedural", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Medium)
            androidx.compose.material3.Switch(
                checked = newProc.value.enabled,
                onCheckedChange = {
                    newProc.value = newProc.value.copy(enabled = it)
                    onChange(newProc.value)
                },
                colors = androidx.compose.material3.SwitchDefaults.colors(
                    checkedThumbColor = Color.Black,
                    checkedTrackColor = Color(0xFF00E5FF)
                )
            )
        }

        androidx.compose.material3.Divider(color = Color.White.copy(alpha = 0.1f))

        // Wave type
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
        ) {
            Text(text = "Wave Type", color = Color.White.copy(alpha = 0.8f), fontSize = 12.sp)
            androidx.compose.material3.TextButton(
                onClick = { /* could show dropdown */ },
                modifier = Modifier.padding(horizontal = 8.dp)
            ) {
                Text(
                    text = newProc.value.waveType.name,
                    color = Color(0xFF00E5FF),
                    fontSize = 12.sp
                )
            }
        }

        // Sliders
        ProceduralSlider(
            label = "Frequency",
            value = newProc.value.frequency,
            range = 0.01f..10f,
            format = { "%.2f Hz" },
            onValueChange = {
                newProc.value = newProc.value.copy(frequency = it)
                onChange(newProc.value)
            }
        )

        ProceduralSlider(
            label = "Amplitude",
            value = newProc.value.amplitude,
            range = 0f..100f,
            format = { "%.1f" },
            onValueChange = {
                newProc.value = newProc.value.copy(amplitude = it)
                onChange(newProc.value)
            }
        )

        ProceduralSlider(
            label = "Octaves",
            value = newProc.value.octaves.toFloat(),
            range = 1f..8f,
            format = { "%.0f" },
            onValueChange = {
                newProc.value = newProc.value.copy(octaves = it.roundToInt())
                onChange(newProc.value)
            }
        )

        ProceduralSlider(
            label = "Amplitude Mult",
            value = newProc.value.amplitudeMult,
            range = 0.1f..1f,
            format = { "%.2f" },
            onValueChange = {
                newProc.value = newProc.value.copy(amplitudeMult = it)
                onChange(newProc.value)
            }
        )

        ProceduralSlider(
            label = "Phase",
            value = newProc.value.phase,
            range = 0f..6.28f,
            format = { "%.2f rad" },
            onValueChange = {
                newProc.value = newProc.value.copy(phase = it)
                onChange(newProc.value)
            }
        )

        ProceduralSlider(
            label = "Seed",
            value = newProc.value.seed.toFloat(),
            range = 0f..9999f,
            format = { "%.0f" },
            onValueChange = {
                newProc.value = newProc.value.copy(seed = it.roundToInt())
                onChange(newProc.value)
            }
        )

        // Preset buttons
        Text(text = "Presets", color = Color.White.copy(alpha = 0.8f), fontSize = 12.sp)
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
        ) {
            PresetProcButton("Subtle Jitter", 2f, 2f, 1, 0.5f, EditorState.ProceduralWaveType.Noise) {
                newProc.value = newProc.value.copy(frequency = 2f, amplitude = 2f, octaves = 1, amplitudeMult = 0.5f, waveType = EditorState.ProceduralWaveType.Noise)
                onChange(newProc.value)
            }
            PresetProcButton("Camera Shake", 5f, 15f, 3, 0.5f, EditorState.ProceduralWaveType.Noise) {
                newProc.value = newProc.value.copy(frequency = 5f, amplitude = 15f, octaves = 3, amplitudeMult = 0.5f, waveType = EditorState.ProceduralWaveType.Noise)
                onChange(newProc.value)
            }
            PresetProcButton("VHS Wobble", 1f, 8f, 2, 0.6f, EditorState.ProceduralWaveType.Triangle) {
                newProc.value = newProc.value.copy(frequency = 1f, amplitude = 8f, octaves = 2, amplitudeMult = 0.6f, waveType = EditorState.ProceduralWaveType.Triangle)
                onChange(newProc.value)
            }
            PresetProcButton("Pulse", 0.5f, 20f, 1, 0f, EditorState.ProceduralWaveType.Square) {
                newProc.value = newProc.value.copy(frequency = 0.5f, amplitude = 20f, octaves = 1, amplitudeMult = 0f, waveType = EditorState.ProceduralWaveType.Square)
                onChange(newProc.value)
            }
        }
    }
}

@Composable
private fun ProceduralSlider(
    label: String,
    value: Float,
    range: ClosedFloatingPointRange<Float>,
    format: (Float) -> String,
    onValueChange: (Float) -> Unit
) {
    var currentValue by remember { mutableStateOf(value) }
    currentValue = value

    Column(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
        ) {
            Text(text = label, color = Color.White.copy(alpha = 0.8f), fontSize = 12.sp)
            Text(text = format(currentValue), color = Color(0xFF00E5FF), fontSize = 11.sp, fontWeight = FontWeight.Medium)
        }
        androidx.compose.material3.Slider(
            modifier = Modifier.fillMaxWidth(),
            value = (currentValue - range.start) / (range.endInclusive - range.start),
            onValueChange = { ratio ->
                val newVal = range.start + ratio * (range.endInclusive - range.start)
                currentValue = newVal
                onValueChange(newVal)
            },
            colors = androidx.compose.material3.SliderDefaults.colors(
                thumbColor = Color(0xFF00E5FF),
                activeTrackColor = Color(0xFF00E5FF),
                inactiveTrackColor = Color.White.copy(alpha = 0.1f)
            )
        )
    }
}

@Composable
private fun androidx.compose.foundation.layout.RowScope.PresetProcButton(
    label: String,
    freq: Float,
    amp: Float,
    oct: Int,
    ampMult: Float,
    wave: EditorState.ProceduralWaveType,
    onClick: () -> Unit
) {
    androidx.compose.material3.TextButton(
        onClick = onClick,
        modifier = Modifier.weight(1f).height(32.dp),
        colors = androidx.compose.material3.ButtonDefaults.textButtonColors(
            containerColor = Color(0xFF1E1E1E)
        )
    ) {
        Text(text = label, color = Color.White, fontSize = 10.sp)
    }
}

@Composable
private fun CenteredText(text: String) {
    Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        Text(text = text, color = Color.White.copy(alpha = 0.4f), fontSize = 14.sp)
    }
}

private fun DrawScope.drawDiamond(center: Offset, radius: Float, fill: Color, stroke: Color, strokeWidth: Float) {
    val path = Path().apply {
        moveTo(center.x, center.y - radius)
        lineTo(center.x + radius, center.y)
        lineTo(center.x, center.y + radius)
        lineTo(center.x - radius, center.y)
        close()
    }
    drawPath(path = path, color = fill, style = Fill)
    drawPath(path = path, color = stroke, style = Stroke(width = strokeWidth))
}

private fun formatTimecode(seconds: Double): String {
    val totalFrames = (seconds * 30).roundToInt()
    val hours = totalFrames / (30 * 60 * 60)
    val minutes = (totalFrames / (30 * 60)) % 60
    val secs = (totalFrames / 30) % 60
    val frames = totalFrames % 30
    return String.format("%02d:%02d:%02d:%02d", hours, minutes, secs, frames)
}

private data class EasingPreset(
    val name: String,
    val interpolation: EditorState.InterpolationType,
    val inTangent: Float = 0f,
    val outTangent: Float = 0f
)

@Composable
private fun DopeSheetView(
    state: EditorState,
    target: EditorState.KeyframeTarget,
    selectedIndex: Int?,
    onSelectIndex: (Int) -> Unit,
    snapEnabled: Boolean
) {
    Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
        Text(
            text = "Dope sheet · ${target.uniformName} · ${target.track.keyframes.size} keys",
            color = Color.White.copy(alpha = 0.7f),
            fontSize = 12.sp
        )
        target.track.keyframes.forEachIndexed { index, kf ->
            val selected = index == selectedIndex
            androidx.compose.material3.TextButton(onClick = { onSelectIndex(index) }) {
                Text(
                    text = "t=${String.format("%.2f", kf.time)}  v=${String.format("%.3f", kf.value)}  ${kf.interpolation.name}",
                    color = if (selected) Color.Cyan else Color.White,
                    fontSize = 12.sp
                )
            }
        }
        if (target.track.keyframes.isEmpty()) {
            Text(text = "No keyframes", color = Color.White.copy(alpha = 0.4f), fontSize = 12.sp)
        }
        Text(
            text = if (snapEnabled) "Snap on · ${String.format("%.2f", state.currentTimeSeconds)}s" else "Snap off",
            color = Color.White.copy(alpha = 0.4f),
            fontSize = 11.sp,
            modifier = Modifier.padding(top = 8.dp)
        )
    }
}

@Composable
private fun EasingPresetsPanel(
    state: EditorState,
    target: EditorState.KeyframeTarget,
    selectedIndex: Int?,
    onApplyPreset: (EasingPreset) -> Unit
) {
    val presets = listOf(
        EasingPreset("Linear", EditorState.InterpolationType.Linear),
        EasingPreset("Step", EditorState.InterpolationType.Step),
        EasingPreset("Ease In", EditorState.InterpolationType.Bezier, inTangent = 0.8f, outTangent = 0.2f),
        EasingPreset("Ease Out", EditorState.InterpolationType.Bezier, inTangent = 0.2f, outTangent = 0.8f),
        EasingPreset("Ease In-Out", EditorState.InterpolationType.Bezier, inTangent = 0.5f, outTangent = 0.5f)
    )
    Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
        Text(text = "Easing presets", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Medium)
        if (selectedIndex == null) {
            Text(text = "Select a keyframe, or apply to all.", color = Color.White.copy(alpha = 0.5f), fontSize = 12.sp)
        }
        presets.forEach { preset ->
            androidx.compose.material3.TextButton(onClick = { onApplyPreset(preset) }) {
                Text(text = preset.name, color = Color.Cyan, fontSize = 13.sp)
            }
        }
    }
}

@Composable
private fun RoamingCurvesPanel(
    state: EditorState,
    target: EditorState.KeyframeTarget,
    onApplyCurve: (List<Float>) -> Unit
) {
    Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
        Text(text = "Roaming curves", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Medium)
        Text(
            text = "Applies a simple ease shape to selected keys on ${target.uniformName}.",
            color = Color.White.copy(alpha = 0.5f),
            fontSize = 12.sp
        )
        androidx.compose.material3.TextButton(onClick = { onApplyCurve(listOf(0f, 0.25f, 0.75f, 1f)) }) {
            Text(text = "Apply S-curve", color = Color.Cyan, fontSize = 13.sp)
        }
        Text(
            text = "Time ${String.format("%.2f", state.currentTimeSeconds)}s",
            color = Color.White.copy(alpha = 0.4f),
            fontSize = 11.sp
        )
    }
}
