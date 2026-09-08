package com.vfxengine.app.ui.common

import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.gestures.detectTransformGestures
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.unit.dp
import com.vfxengine.app.ui.common.EditorState

/**
 * Gesture shortcuts for the node editor:
 * - Double-tap: Add node menu at position
 * - Pinch: Zoom
 * - Two-finger drag: Pan
 * - Space: Play/Pause
 * - Delete: Remove selected node
 * - Ctrl+Z: Undo
 * - Ctrl+Y / Ctrl+Shift+Z: Redo
 * - Ctrl+C/V: Copy/Paste
 * - Ctrl+A: Select all
 * - +/-: Zoom in/out
 * - 0: Fit to view
 * - Space+ drag: Hand tool (pan)
 */

fun Modifier.nodeEditorGestures(
    state: EditorState,
    onAddNodeRequested: (Offset) -> Unit = {},
    onFitToView: () -> Unit = {}
): Modifier = this.then(
    Modifier.pointerInput(Unit) {
        detectTransformGestures { centroid, pan, zoom, _ ->
            state.nodeZoom = (state.nodeZoom * zoom).coerceIn(0.1f, 5.0f)
            state.nodePanX += pan.x
            state.nodePanY += pan.y
        }
    }
).pointerInput(Unit) {
    detectTapGestures(
        onDoubleTap = { offset ->
            onAddNodeRequested(offset)
        }
    }
)

fun Modifier.timelineGestures(
    state: EditorState,
    onSeekRequested: (Double) -> Unit = {}
): Modifier = this.then(
    Modifier.pointerInput(Unit) {
        detectTapGestures(
            onTap = { offset ->
                val seconds = (offset.x / state.timeScale) + state.timeOffset
                onSeekRequested(seconds.coerceIn(0.0, state.durationSeconds))
            }
        )
    }
)
