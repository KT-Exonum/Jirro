package com.vfxengine.app.ui.tab

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
import androidx.compose.material3.Card
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
 * Fusion Tab - Node Graph Screen
 * Header: Node graph title with zoom controls
 * Canvas: Node graph with connections
 * Warning banner
 * Inspector panel at bottom
 */
@Composable
fun FusionTab(state: EditorState) {
    var showWarning by remember { mutableStateOf(true) }
    var showTools by remember { mutableStateOf(true) }
    var showInspector by remember { mutableStateOf(true) }

    Box(modifier = Modifier.fillMaxSize()) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Header with zoom controls
            FusionHeader()

            // Main content area with tool palette, node canvas, and inspector
            Row(modifier = Modifier.fillMaxSize().weight(1f)) {
                // Left sidebar: Tools
                if (showTools) {
                    FusionToolPalette(state)
                    androidx.compose.foundation.layout.Divider(color = Color.White.copy(alpha = 0.1f))
                }

                // Node canvas (main area)
                FusionNodeCanvas(state)

                // Right sidebar: Inspector
                if (showInspector) {
                    androidx.compose.foundation.layout.Divider(color = Color.White.copy(alpha = 0.1f))
                    InspectorPanel(state)
                }
            }

            // Warning banner
            if (showWarning) {
                WarningBanner(onDismiss = { showWarning = false })
            }
        }
        
        // Floating tool toggle button
        if (!showTools) {
            FloatingToolToggle(onClick = { showTools = true })
        }
    }
}

@Composable
fun FloatingToolToggle(onClick: () -> Unit) {
    Box(
        modifier = Modifier
            .align(Alignment.BottomStart)
            .padding(16.dp),
        contentAlignment = Alignment.Center
    ) {
        androidx.compose.material3.FloatingActionButton(
            onClick = onClick,
            containerColor = Color.Cyan,
            contentColor = Color.Black
        ) {
            Icon(
                painter = painterResource(id = android.R.drawable.ic_menu_manage),
                contentDescription = "Show Tools",
                tint = Color.Black
            )
        }
    }
}

@Composable
fun FusionTab(state: EditorState) {
    var showWarning by remember { mutableStateOf(true) }
    var showTools by remember { mutableStateOf(true) }
    var showInspector by remember { mutableStateOf(true) }

    Box(modifier = Modifier.fillMaxSize()) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Header with zoom controls
            FusionHeader()

            // Main content area with tool palette, node canvas, and inspector
            Row(modifier = Modifier.fillMaxSize().weight(1f)) {
                // Left sidebar: Tools
                if (showTools) {
                    FusionToolPalette(state)
                    androidx.compose.foundation.layout.Divider(color = Color.White.copy(alpha = 0.1f))
                }

                // Node canvas (main area)
                FusionNodeCanvas(state)

                // Right sidebar: Inspector
                if (showInspector) {
                    androidx.compose.foundation.layout.Divider(color = Color.White.copy(alpha = 0.1f))
                    InspectorPanel(state)
                }
            }

            // Warning banner
            if (showWarning) {
                WarningBanner(onDismiss = { showWarning = false })
            }
        }
        
        // Floating tool toggle button
        if (!showTools) {
            FloatingToolToggle(onClick = { showTools = true })
        }
    }
}

/**
 * Tool palette sidebar with all node categories
 */
@Composable
fun FusionToolPalette(state: EditorState) {
    Card(
        modifier = Modifier
            .width(280.dp)
            .fillMaxHeight()
            .background(Color(0xFF121212))
            .padding(0.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Tool palette header
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(48.dp)
                    .padding(horizontal = 16.dp)
                    .background(Color(0xFF1E1E1E)),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(text = "Tools", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                IconButton(onClick = { /* collapse */ }) {
                    Icon(
                        painter = painterResource(id = android.R.drawable.ic_media_rew),
                        contentDescription = "Collapse",
                        tint = Color.White
                    )
                }
            }

            Divider(color = Color.White.copy(alpha = 0.1f))

            // Tool categories
            androidx.compose.foundation.lazy.LazyColumn(
                modifier = Modifier.fillMaxSize().padding(8.dp),
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
            ) {
                // Sources
                ToolCategorySection(
                    title = "Sources",
                    color = 0xFF2196F3.toInt(),
                    items = listOf(
                        ToolItem("Video", EditorState.NodeType.VideoSource, android.R.drawable.ic_media_play),
                        ToolItem("Image", EditorState.NodeType.ImageSource, android.R.drawable.ic_menu_gallery),
                        ToolItem("Audio", EditorState.NodeType.AudioSource, android.R.drawable.ic_media_play),
                        ToolItem("Vector", EditorState.NodeType.VectorSource, android.R.drawable.ic_menu_gallery),
                        ToolItem("Text", EditorState.NodeType.TextSource, android.R.drawable.ic_menu_edit),
                        ToolItem("Stroke", EditorState.NodeType.StrokeSource, android.R.drawable.ic_menu_edit),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Shape2D (DaVinci Resolve style)
                ToolCategorySection(
                    title = "Shape2D",
                    color = 0xFF4CAF50.toInt(),
                    items = listOf(
                        ToolItem("Rectangle", EditorState.NodeType.ShapeRectangle, android.R.drawable.ic_menu_crop),
                        ToolItem("Ellipse", EditorState.NodeType.ShapeEllipse, android.R.drawable.ic_menu_crop),
                        ToolItem("Polygon", EditorState.NodeType.ShapePolygon, android.R.drawable.ic_menu_crop),
                        ToolItem("Star", EditorState.NodeType.ShapeStar, android.R.drawable.ic_menu_crop),
                        ToolItem("Path", EditorState.NodeType.ShapePath, android.R.drawable.ic_menu_edit),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                ShapeOperationCategorySection(
                    title = "Shape Operations",
                    color = 0xFF8BC34A.toInt(),
                    items = listOf(
                        ToolItem("Render", EditorState.NodeType.ShapeRender, android.R.drawable.ic_media_play),
                        ToolItem("Merge", EditorState.NodeType.ShapeMerge, android.R.drawable.ic_menu_agenda),
                        ToolItem("Transform", EditorState.NodeType.ShapeTransform, android.R.drawable.ic_menu_crop),
                        ToolItem("Stroke", EditorState.NodeType.ShapeStroke, android.R.drawable.ic_menu_edit),
                        ToolItem("Fill", EditorState.NodeType.ShapeFill, android.R.drawable.ic_menu_edit),
                        ToolItem("Repeater", EditorState.NodeType.ShapeRepeater, android.R.drawable.ic_menu_agenda),
                        ToolItem("Boolean", EditorState.NodeType.ShapeBoolean, android.R.drawable.ic_menu_agenda),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // 2.5D System (Z-axis for 2D planes)
                ToolCategorySection(
                    title = "2.5D / 3D",
                    color = 0xFF673AB7.toInt(),
                    items = listOf(
                        ToolItem("Transform 3D", EditorState.NodeType.Transform3D, android.R.drawable.ic_menu_crop),
                        ToolItem("Camera 3D", EditorState.NodeType.Camera3D, android.R.drawable.ic_menu_mapmode),
                        ToolItem("Depth of Field", EditorState.NodeType.DepthOfField, android.R.drawable.ic_menu_rotate),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Filters
                ToolCategorySection(
                    title = "Filters",
                    color = 0xFF2196F3.toInt(),
                    items = listOf(
                        ToolItem("Blur", EditorState.NodeType.Blur, android.R.drawable.ic_menu_rotate),
                        ToolItem("Motion Blur", EditorState.NodeType.MotionBlur, android.R.drawable.ic_media_ff),
                        ToolItem("Directional Blur", EditorState.NodeType.DirectionalBlur, android.R.drawable.ic_media_ff),
                        ToolItem("Transform Blur", EditorState.NodeType.TransformBlur, android.R.drawable.ic_menu_crop),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Color
                ToolCategorySection(
                    title = "Color",
                    color = 0xFFF44336.toInt(),
                    items = listOf(
                        ToolItem("Color Correction", EditorState.NodeType.ColorCorrection, android.R.drawable.ic_menu_gallery),
                        ToolItem("Adjustment", EditorState.NodeType.Adjustment, android.R.drawable.ic_menu_edit),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Composite
                ToolCategorySection(
                    title = "Composite",
                    color = 0xFF3F51B5.toInt(),
                    items = listOf(
                        ToolItem("Blend", EditorState.NodeType.Blend, android.R.drawable.ic_media_ff),
                        ToolItem("Composite", EditorState.NodeType.Composite, android.R.drawable.ic_menu_agenda),
                        ToolItem("Mask", EditorState.NodeType.Mask, android.R.drawable.ic_menu_crop),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Time/Velocity
                ToolCategorySection(
                    title = "Time & Velocity",
                    color = 0xFF00BCD4.toInt(),
                    items = listOf(
                        ToolItem("Velocity Graph", EditorState.NodeType.VelocityGraph, android.R.drawable.ic_media_ff),
                        ToolItem("Time Remap", EditorState.NodeType.TimeRemap, android.R.drawable.ic_media_rew),
                        ToolItem("Optical Flow", EditorState.NodeType.OpticalFlow, android.R.drawable.ic_menu_rotate),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Masking/Rotoscoping
                ToolCategorySection(
                    title = "Masking & Roto",
                    color = 0xFF9C27B0.toInt(),
                    items = listOf(
                        ToolItem("Bezier Mask", EditorState.NodeType.BezierMask, android.R.drawable.ic_menu_crop),
                        ToolItem("Rotoscoping", EditorState.NodeType.Rotoscoping, android.R.drawable.ic_menu_edit),
                        ToolItem("Roto Brush", EditorState.NodeType.RotoBrush, android.R.drawable.ic_menu_edit),
                        ToolItem("Tracker", EditorState.NodeType.Tracker, android.R.drawable.ic_menu_mapmode),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Particles
                ToolCategorySection(
                    title = "Particles",
                    color = 0xFFFF5722.toInt(),
                    items = listOf(
                        ToolItem("Emitter", EditorState.NodeType.ParticleEmitter, android.R.drawable.ic_menu_add),
                        ToolItem("Forces", EditorState.NodeType.ParticleForces, android.R.drawable.ic_menu_rotate),
                        ToolItem("Renderer", EditorState.NodeType.ParticleRenderer, android.R.drawable.ic_media_play),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Utility
                ToolCategorySection(
                    title = "Utility",
                    color = 0xFF8BC34A.toInt(),
                    items = listOf(
                        ToolItem("Shader", EditorState.NodeType.Shader, android.R.drawable.ic_menu_edit),
                        ToolItem("Null", EditorState.NodeType.Null, android.R.drawable.ic_menu_help),
                        ToolItem("Output", EditorState.NodeType.Output, android.R.drawable.ic_media_next),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )
            }
        }
    }
}

data class ToolItem(val label: String, val nodeType: EditorState.NodeType, val iconRes: Int)

@Composable
fun ToolCategorySection(
    title: String,
    color: Int,
    items: List<ToolItem>,
    onItemClick: (EditorState.NodeType) -> Unit
) {
    Column(modifier = Modifier.fillMaxWidth()) {
        Text(text = title, color = Color(color), fontSize = 11.sp, fontWeight = FontWeight.Bold, modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp))
        androidx.compose.foundation.lazy.LazyColumn(
            modifier = Modifier.fillMaxWidth(),
            verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(4.dp)
        ) {
            items(items) { item ->
                androidx.compose.material3.TextButton(
                    onClick = { onItemClick(item.nodeType) },
                    modifier = Modifier.fillMaxWidth().height(36.dp).padding(horizontal = 12.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Start,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(
                            painter = painterResource(id = item.iconRes),
                            contentDescription = "",
                            tint = Color(color),
                            modifier = Modifier.size(20.dp)
                        )
                        androidx.compose.foundation.layout.Box(modifier = Modifier.width(12.dp))
                        Text(text = item.label, color = Color.White, fontSize = 12.sp)
                    }
                }
            }
        }
    }
}

@Composable
fun ShapeOperationCategorySection(
    title: String,
    color: Int,
    items: List<ToolItem>,
    onItemClick: (EditorState.NodeType) -> Unit
) {
    ToolCategorySection(title, color, items, onItemClick)
}

fun addNodeOfType(state: EditorState, type: EditorState.NodeType) {
    state.addNode(type, 200f, 200f)
}

@Composable
fun FloatingToolToggle(onClick: () -> Unit) {
    Box(
        modifier = Modifier
            .align(Alignment.BottomStart)
            .padding(16.dp),
        contentAlignment = Alignment.Center
    ) {
        androidx.compose.material3.FloatingActionButton(
            onClick = onClick,
            containerColor = Color.Cyan,
            contentColor = Color.Black
        ) {
            Icon(
                painter = painterResource(id = android.R.drawable.ic_menu_manage),
                contentDescription = "Show Tools",
                tint = Color.Black
            )
        }
    }
}

@Composable
fun FusionHeader() {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(48.dp)
            .padding(horizontal = 16.dp)
            .background(Color(0xFF121212)),
        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Title
        Text(text = "Node graph", color = Color.White, fontSize = 18.sp, fontWeight = FontWeight.Bold)

        // Zoom controls
        Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(4.dp)) {
            Text(text = "Zoom", color = Color.White.copy(alpha = 0.6f), fontSize = 12.sp)
            IconButton(onClick = { /* zoom out */ }) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_media_rew),
                    contentDescription = "Zoom Out",
                    tint = Color.White
                )
            }
            IconButton(onClick = { /* zoom in */ }) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_media_ff),
                    contentDescription = "Zoom In",
                    tint = Color.White
                )
            }
        }

        // 3D Viewport controls
        androidx.compose.foundation.layout.Box(modifier = Modifier.width(16.dp))
        Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(4.dp)) {
            Text(text = "View", color = Color.White.copy(alpha = 0.6f), fontSize = 12.sp)
            IconButton(onClick = { /* view: perspective */ }) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_zoom),
                    contentDescription = "Perspective",
                    tint = Color.Cyan
                )
            }
            IconButton(onClick = { /* view: orthographic */ }) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_crop),
                    contentDescription = "Orthographic",
                    tint = Color.White
                )
            }
            IconButton(onClick = { /* view: top */ }) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_upload),
                    contentDescription = "Top",
                    tint = Color.White
                )
            }
            IconButton(onClick = { /* view: front */ }) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_upload),
                    contentDescription = "Front",
                    tint = Color.White
                )
            }
            IconButton(onClick = { /* view: right */ }) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_upload),
                    contentDescription = "Right",
                    tint = Color.White
                )
            }
        }

        // Navigation mode
        androidx.compose.foundation.layout.Box(modifier = Modifier.width(16.dp))
        Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(4.dp)) {
            Text(text = "Nav", color = Color.White.copy(alpha = 0.6f), fontSize = 12.sp)
            IconButton(onClick = { /* orbit */ }) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_rotate),
                    contentDescription = "Orbit",
                    tint = Color.Cyan
                )
            }
            IconButton(onClick = { /* pan */ }) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_crop),
                    contentDescription = "Pan",
                    tint = Color.White
                )
            }
            IconButton(onClick = { /* dolly */ }) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_media_ff),
                    contentDescription = "Dolly",
                    tint = Color.White
                )
            }
        }
    }
}

@Composable
fun FusionNodeCanvas(state: EditorState) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .weight(1f)
            .background(Color(0xFF0D0D0D))
            .pointerInput(state) {
                detectDragGestures(
                    onDragStart = { },
                    onDrag = { change, dragAmount ->
                        state.nodePanX += dragAmount.x
                        state.nodePanY += dragAmount.y
                    },
                    onDragEnd = { }
                )
                detectTapGestures(
                    onTap = { offset ->
                        state.selectedNodeId = null
                    }
                )
            }
            .graphicsLayer {
                scaleX = state.nodeZoom
                scaleY = state.nodeZoom
                translationX = state.nodePanX.px
                translationY = state.nodePanY.px
            }
    ) {
        // Draw connections
        state.connections.value.forEach { conn ->
            ConnectionLine(state, conn)
        }

        // Draw nodes
        state.nodes.value.values.forEach { node ->
            FusionNodeView(state, node)
        }

        // Draw connection preview
        state.dragState?.let { drag ->
            when (drag) {
                is EditorState.DragState.Connecting -> {
                    val fromNode = state.nodes.value[drag.fromNodeId]
                    fromNode?.let { node ->
                        val fromX = node.x + 180f + state.nodePanX * state.nodeZoom
                        val fromY = node.y + 20 + node.outputs.indexOfFirst { it.name == drag.fromPort } * 36f * state.nodeZoom + state.nodePanY * state.nodeZoom

                        Canvas(modifier = Modifier.fillMaxSize()) {
                            val path = Path()
                            path.moveTo(fromX, fromY)
                            val ctrlX1 = fromX + 80
                            val ctrlX2 = drag.currentX - 80
                            path.cubicTo(ctrlX1, fromY, ctrlX2, drag.currentY, drag.currentX, drag.currentY)
                            drawPath(
                                path = path,
                                color = Color.Cyan.copy(alpha = 0.4f),
                                style = androidx.compose.ui.graphics.Stroke(width = 2f, cap = androidx.compose.ui.graphics.StrokeCap.Round)
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun FusionNodeView(state: EditorState, node: EditorState.Node) {
    val isSelected = state.selectedNodeId == node.id
    val maxPorts = maxOf(node.inputs.size, node.outputs.size)
    val nodeHeight = max(100f, maxPorts * 40f + 50f)
    val nodeWidth = 200f

    Box(
        modifier = Modifier
            .width(nodeWidth.dp)
            .height(nodeHeight.dp)
            .background(if (isSelected) Color(0xFF2A3A4A) else Color(0xFF1E1E1E))
            .graphicsLayer {
                translationX = node.x.px
                translationY = node.y.px
            }
            .pointerInput(node) {
                detectTapGestures(
                    onTap = { state.selectedNodeId = node.id }
                )
                detectDragGestures(
                    onDragStart = { },
                    onDrag = { change, dragAmount ->
                        node.x += dragAmount.x
                        node.y += dragAmount.y
                    },
                    onDragEnd = { }
                )
            }
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(8.dp)) {
            // Node header
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
            ) {
                Text(text = node.name, color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                Box(
                    modifier = Modifier
                        .width(12.dp)
                        .height(12.dp)
                        .background(Color(node.type.color))
                )
            }

            // Type badge
            Text(text = node.type.label, color = Color(node.type.color), fontSize = 9.sp, modifier = Modifier.fillMaxWidth().padding(top = 2.dp))

            // Ports
            val portCount = maxOf(node.inputs.size, node.outputs.size)
            repeat(portCount) { i ->
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
                ) {
                    // Input port
                    if (i < node.inputs.size) {
                        FusionPort(
                            state = state,
                            node = node,
                            port = node.inputs[i],
                            isOutput = false,
                            index = i
                        )
                    } else {
                        Box(modifier = Modifier.size(16.dp))
                    }

                    // Output port
                    if (i < node.outputs.size) {
                        FusionPort(
                            state = state,
                            node = node,
                            port = node.outputs[i],
                            isOutput = true,
                            index = i
                        )
                    } else {
                        Box(modifier = Modifier.size(16.dp))
                    }
                }
            }
        }
    }
}

@Composable
fun FusionPort(
    state: EditorState,
    node: EditorState.Node,
    port: EditorState.Port,
    isOutput: Boolean,
    index: Int
) {
    val isConnected = state.connections.value.any { c ->
        if (isOutput) c.fromNodeId == node.id && c.fromPort == port.name
        else c.toNodeId == node.id && c.toPort == port.name
    }

    val portColor = if (isConnected) Color.Cyan else Color.Gray

    Box(
        modifier = Modifier
            .size(16.dp)
            .background(portColor)
            .graphicsLayer {
                if (isOutput) translationX = (200f - 16).px else translationX = 0.px
            }
            .pointerInput(Unit) {
                detectDragGestures(
                    onDragStart = {
                        state.dragState = EditorState.DragState.Connecting(
                            node.id,
                            port.name,
                            0f,
                            0f
                        )
                    },
                    onDrag = { change, dragAmount ->
                        state.dragState = EditorState.DragState.Connecting(
                            node.id,
                            port.name,
                            change.position.x,
                            change.position.y
                        )
                    },
                    onDragEnd = { state.dragState = null }
                )
            }
    )
}

@Composable
fun ConnectionLine(state: EditorState, connection: EditorState.Connection) {
    val fromNode = state.nodes.value[connection.fromNodeId]
    val toNode = state.nodes.value[connection.toNodeId]

    fromNode?.let { from ->
        toNode?.let { to ->
            val fromX = from.x + 180f
            val fromY = from.y + 20 + from.outputs.indexOfFirst { it.name == connection.fromPort } * 36f
            val toX = to.x
            val toY = to.y + 20 + to.inputs.indexOfFirst { it.name == connection.toPort } * 36f

            Canvas(modifier = Modifier.fillMaxSize()) {
                val path = Path()
                path.moveTo(fromX, fromY)
                val ctrlX1 = fromX + 80
                val ctrlX2 = toX - 80
                path.cubicTo(ctrlX1, fromY, ctrlX2, toY, toX, toY)
                drawPath(
                    path = path,
                    color = Color.White.copy(alpha = 0.6f),
                    style = androidx.compose.ui.graphics.Stroke(width = 2f)
                )
            }
        }
    }
}

@Composable
fun WarningBanner(onDismiss: () -> Unit) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp)
            .background(Color(0xFF3A2A0D))
    ) {
        Row(
            modifier = Modifier.fillMaxSize().padding(12.dp),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(
                painter = painterResource(id = android.R.drawable.ic_dialog_alert),
                contentDescription = "Warning",
                tint = Color.Yellow
            )
            androidx.compose.foundation.layout.Box(modifier = Modifier.width(8.dp))
            Text(
                text = "Ports sit ~10px apart at this zoom — dragging a new connection between them is unreliable with a finger. Pinch to zoom in before connecting.",
                color = Color.White,
                fontSize = 12.sp,
                modifier = Modifier.weight(1f)
            )
            IconButton(onClick = onDismiss) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_close_clear_cancel),
                    contentDescription = "Dismiss",
                    tint = Color.White.copy(alpha = 0.7f)
                )
            }
        }
    }
}

@Composable
fun InspectorPanel(state: EditorState) {
    val selectedNode = state.selectedNodeId?.let { state.nodes.value[it] }

    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(200.dp)
            .background(Color(0xFF121212))
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            // Inspector header
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
            ) {
                Text(
                    text = "Inspector — ${selectedNode?.name ?: "No selection"}",
                    color = Color.White,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold
                )
                if (selectedNode != null) {
                    Text(
                        text = selectedNode.type.label,
                        color = Color(selectedNode.type.color),
                        fontSize = 11.sp
                    )
                }
            }

            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))

            if (selectedNode == null) {
                androidx.compose.foundation.layout.Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    Text(text = "Select a node to inspect", color = Color.White.copy(alpha = 0.5f), fontSize = 14.sp)
                }
            } else {
                // Uniform controls
                Column(
                    modifier = Modifier.fillMaxSize(),
                    verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
                ) {
                    // Gain slider
                    UniformSlider(
                        label = "Gain",
                        value = selectedNode.uniforms["gain"] ?: 1.0f,
                        min = 0f,
                        max = 3f,
                        onValueChange = { value ->
                            state.updateNodeUniform(selectedNode.id, "gain", value)
                        }
                    )

                    // Lift slider
                    UniformSlider(
                        label = "Lift",
                        value = selectedNode.uniforms["lift"] ?: 0f,
                        min = -1f,
                        max = 1f,
                        onValueChange = { value ->
                            state.updateNodeUniform(selectedNode.id, "lift", value)
                        }
                    )
                }
            }
        }
    }
}

@Composable
fun UniformSlider(
    label: String,
    value: Float,
    min: Float,
    max: Float,
    onValueChange: (Float) -> Unit
) {
    var currentValue by remember { mutableStateOf(value) }

    Column(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
        ) {
            Text(text = label, color = Color.White, fontSize = 13.sp)
            Text(text = "%.2f".format(currentValue), color = Color.Cyan, fontSize = 13.sp, fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace)
        }

        Slider(
            modifier = Modifier.fillMaxWidth(),
            value = (currentValue - min) / (max - min),
            onValueChange = { ratio ->
                val newValue = min + ratio * (max - min)
                currentValue = newValue
                onValueChange(newValue)
            },
            colors = androidx.compose.material3.SliderDefaults.colors(
                thumbColor = Color.Cyan,
                activeTrackColor = Color.Cyan,
                inactiveTrackColor = Color.White.copy(alpha = 0.2f)
            )
        )
    }
}