package com.vfxengine.app.ui.common

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.px
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.px

/**
 * A resizable panel that can be dragged to adjust its size.
 * Supports both horizontal and vertical resizing.
 */
@Composable
fun ResizablePanel(
    modifier: Modifier = Modifier,
    initialSize: Float,
    minSize: Float = 100f,
    maxSize: Float = Float.MAX_VALUE,
    isHorizontal: Boolean = true, // true = vertical divider (resizes width), false = horizontal divider (resizes height)
    onSizeChange: (Float) -> Unit,
    content: @Composable (Modifier) -> Unit
) {
    val currentSize = remember { mutableStateOf(initialSize) }
    val isDragging = remember { mutableStateOf(false) }

    // Update initial size if it changes externally
    if (initialSize != currentSize.value && !isDragging.value) {
        currentSize.value = initialSize.coerceIn(minSize, maxSize)
    }

    val panelModifier = if (isHorizontal) {
        modifier.width(currentSize.value.dp).fillMaxHeight()
    } else {
        modifier.height(currentSize.value.dp).fillMaxWidth()
    }

    val dividerSize = 8.dp
    val dividerModifier = if (isHorizontal) {
        Modifier.width(dividerSize).fillMaxHeight()
    } else {
        Modifier.height(dividerSize).fillMaxWidth()
    }

    Row(modifier = modifier.fillMaxSize()) {
        // Content panel
        Box(
            modifier = panelModifier
                .background(Color(0xFF121212))
        ) {
            content(Modifier.fillMaxSize())
        }

        // Divider handle
        Box(
            modifier = dividerModifier
                .background(if (isDragging.value) Color.Cyan else Color.White.copy(alpha = 0.1f))
                .pointerInput(Unit) {
                    detectDragGestures(
                        onDragStart = { isDragging.value = true },
                        onDrag = { change, dragAmount ->
                            val delta = if (isHorizontal) dragAmount.x else dragAmount.y
                            val newSize = (currentSize.value + delta / 1f).coerceIn(minSize, maxSize)
                            currentSize.value = newSize
                            onSizeChange(newSize)
                        },
                        onDragEnd = { isDragging.value = false }
                    )
                }
        ) {
            // Drag indicator lines
            if (isHorizontal) {
                Column(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(horizontal = 2.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    repeat(3) {
                        Box(
                            modifier = Modifier
                                .width(2.dp)
                                .height(12.dp)
                                .background(Color.White.copy(alpha = 0.3f))
                                .padding(vertical = 4.dp)
                        )
                    }
                }
            } else {
                Row(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(vertical = 2.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    repeat(3) {
                        Box(
                            modifier = Modifier
                                .height(2.dp)
                                .width(12.dp)
                                .background(Color.White.copy(alpha = 0.3f))
                                .padding(horizontal = 4.dp)
                        )
                    }
                }
            }
        }
    }
}

/**
 * Layout with two resizable panels side by side (horizontal split)
 */
@Composable
fun HorizontalSplit(
    modifier: Modifier = Modifier,
    leftInitialSize: Float = 400f,
    rightInitialSize: Float = 400f,
    minSize: Float = 200f,
    onLeftSizeChange: (Float) -> Unit = {},
    onRightSizeChange: (Float) -> Unit = {},
    leftContent: @Composable (Modifier) -> Unit,
    rightContent: @Composable (Modifier) -> Unit
) {
    var leftSize by remember { mutableStateOf(leftInitialSize) }
    var rightSize by remember { mutableStateOf(rightInitialSize) }

    ResizablePanel(
        modifier = modifier.fillMaxSize(),
        initialSize = leftSize,
        minSize = minSize,
        isHorizontal = true,
        onSizeChange = { newSize ->
            leftSize = newSize
            onLeftSizeChange(newSize)
        }
    ) { contentModifier ->
        leftContent(contentModifier)
    }

    // Right panel takes remaining space
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .fillMaxHeight()
            .background(Color(0xFF121212))
    ) {
        rightContent(Modifier.fillMaxSize())
    }
}

/**
 * Layout with two resizable panels stacked vertically
 */
@Composable
fun VerticalSplit(
    modifier: Modifier = Modifier,
    topInitialSize: Float = 400f,
    bottomInitialSize: Float = 400f,
    minSize: Float = 100f,
    onTopSizeChange: (Float) -> Unit = {},
    onBottomSizeChange: (Float) -> Unit = {},
    topContent: @Composable (Modifier) -> Unit,
    bottomContent: @Composable (Modifier) -> Unit
) {
    var topSize by remember { mutableStateOf(topInitialSize) }
    var bottomSize by remember { mutableStateOf(bottomInitialSize) }

    Column(modifier = modifier.fillMaxSize()) {
        ResizablePanel(
            modifier = Modifier.fillMaxWidth(),
            initialSize = topSize,
            minSize = minSize,
            isHorizontal = false,
            onSizeChange = { newSize ->
                topSize = newSize
                onTopSizeChange(newSize)
            }
        ) { contentModifier ->
            topContent(contentModifier)
        }

        // Bottom panel takes remaining space
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight()
                .background(Color(0xFF121212))
        ) {
            bottomContent(Modifier.fillMaxSize())
        }
    }
}

/**
 * Three-pane horizontal layout with two resizable dividers
 */
@Composable
fun ThreePaneHorizontal(
    modifier: Modifier = Modifier,
    leftInitialSize: Float = 400f,
    centerInitialSize: Float = 600f,
    rightInitialSize: Float = 300f,
    minSize: Float = 200f,
    onLeftSizeChange: (Float) -> Unit = {},
    onCenterSizeChange: (Float) -> Unit = {},
    onRightSizeChange: (Float) -> Unit = {},
    leftContent: @Composable (Modifier) -> Unit,
    centerContent: @Composable (Modifier) -> Unit,
    rightContent: @Composable (Modifier) -> Unit
) {
    var leftSize by remember { mutableStateOf(leftInitialSize) }
    var centerSize by remember { mutableStateOf(centerInitialSize) }
    var rightSize by remember { mutableStateOf(rightInitialSize) }

    Row(modifier = modifier.fillMaxSize()) {
        // Left panel
        ResizablePanel(
            modifier = Modifier.fillMaxHeight(),
            initialSize = leftSize,
            minSize = minSize,
            isHorizontal = true,
            onSizeChange = { newSize ->
                leftSize = newSize
                onLeftSizeChange(newSize)
            }
        ) { contentModifier ->
            leftContent(contentModifier)
        }

        // Center panel
        ResizablePanel(
            modifier = Modifier.fillMaxHeight(),
            initialSize = centerSize,
            minSize = minSize,
            isHorizontal = true,
            onSizeChange = { newSize ->
                centerSize = newSize
                onCenterSizeChange(newSize)
            }
        ) { contentModifier ->
            centerContent(contentModifier)
        }

        // Right panel (fills remaining)
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight()
                .background(Color(0xFF121212))
        ) {
            rightContent(Modifier.fillMaxSize())
        }
    }
}