package com.vfxengine.app.ui.tab

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
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
 * Fusion Tab - DaVinci Resolve style Fusion page
 * Full-screen node editor with inspector, keyframe editor, spline editor
 */
@Composable
fun FusionTab(state: EditorState) {
    var showInspector by remember { mutableStateOf(true) }
    var showKeyframes by remember { mutableStateOf(false) }
    var showSpline by remember { mutableStateOf(false) }
    var showNodes by remember { mutableStateOf(true) }
    
    Box(modifier = Modifier.fillMaxSize()) {
        // Main node editor canvas (center)
        if (showNodes) {
            FusionNodeCanvas(state)
        }
        
        // Left sidebar: Tools + Node library
        if (showNodes) {
            FusionLeftSidebar(state)
        }
        
        // Right sidebar: Inspector / Keyframes / Spline
        if (showInspector || showKeyframes || showSpline) {
            FusionRightSidebar(state, showInspector, showKeyframes, showSpline)
        }
        
        // Top toolbar
        FusionToolbar(
            state = state,
            showInspector = showInspector,
            onInspectorToggle = { showInspector = !showInspector },
            onKeyframesToggle = { showKeyframes = !showKeyframes; showInspector = false },
            onSplineToggle = { showSpline = !showSpline; showInspector = false },
            onNodesToggle = { showNodes = !showNodes }
        )
        
        // Bottom: Keyframe timeline (when keyframes open)
        if (showKeyframes) {
            FusionKeyframeTimeline(state)
        }
    }
}

@Composable
fun FusionToolbar(
    state: EditorState,
    showInspector: Boolean,
    onInspectorToggle: () -> Unit,
    onKeyframesToggle: () -> Unit,
    onSplineToggle: () -> Unit,
    onNodesToggle: () -> Unit
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
        // Page title
        Text(text = "Fusion", color = Color.White, fontSize = 18.sp, fontWeight = FontWeight.Bold)
        
        androidx.compose.foundation.layout.Box(modifier = Modifier.weight(1f))
        
        // Tool buttons
        Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(4.dp)) {
            FusionTool.values().forEach { tool ->
                androidx.compose.material3.IconButton(
                    onClick = { /* select tool */ },
                    colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                        containerColor = if (state.currentTool == tool) Color.Cyan.copy(alpha = 0.2f) else Color.Transparent
                    )
                ) {
                    Icon(
                        painter = painterResource(id = tool.iconRes),
                        contentDescription = tool.label,
                        tint = if (state.currentTool == tool) Color.Cyan else Color.White
                    )
                }
            }
        }
        
        androidx.compose.foundation.layout.Box(modifier = Modifier.width(16.dp))
        
        // View toggles
        Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(4.dp)) {
            IconButton(
                onClick = onNodesToggle,
                colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                    containerColor = Color.Cyan.copy(alpha = 0.2f)
                )
            ) {
                Icon(painter = painterResource(id = android.R.drawable.ic_menu_manage), contentDescription = "Nodes", tint = Color.Cyan)
            }
            IconButton(
                onClick = onInspectorToggle,
                colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                    containerColor = if (showInspector) Color.Cyan.copy(alpha = 0.2f) else Color.Transparent
                )
            ) {
                Icon(painter = painterResource(id = android.R.drawable.ic_menu_info_details), contentDescription = "Inspector", tint = if (showInspector) Color.Cyan else Color.White)
            }
            IconButton(
                onClick = onKeyframesToggle,
                colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                    containerColor = if (showKeyframes) Color.Cyan.copy(alpha = 0.2f) else Color.Transparent
                )
            ) {
                Icon(painter = painterResource(id = android.R.drawable.ic_media_play), contentDescription = "Keyframes", tint = if (showKeyframes) Color.Cyan else Color.White)
            }
            IconButton(
                onClick = onSplineToggle,
                colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                    containerColor = if (showSpline) Color.Cyan.copy(alpha = 0.2f) else Color.Transparent
                )
            ) {
                Icon(painter = painterResource(id = android.R.drawable.ic_menu_report_image), contentDescription = "Spline", tint = if (showSpline) Color.Cyan else Color.White)
            }
        }
    }
}

enum class FusionTool(val label: String, val iconRes: Int) {
    Select("Select", android.R.drawable.ic_menu_selectall),
    Pan("Pan", android.R.drawable.ic_menu_mapmode),
    Zoom("Zoom", android.R.drawable.ic_menu_zoom),
    Connect("Connect", android.R.drawable.ic_media_ff),
    Cut("Cut", android.R.drawable.ic_menu_crop)
}

@Composable
fun FusionLeftSidebar(state: EditorState) {
    Card(
        modifier = Modifier
            .width(280.dp)
            .fillMaxHeight()
            .background(Color(0xFF121212))
            .padding(0.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Tools palette
            Text(text = "Tools", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp, fontWeight = FontWeight.Bold, modifier = Modifier.padding(16.dp))
            
            LazyColumn(
                modifier = Modifier.fillMaxSize().padding(8.dp),
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
            ) {
                items(ToolCategory.values()) { category ->
                    ToolCategoryItem(category = category, onClick = { /* expand category */ })
                }
            }
        }
    }
}

enum class ToolCategory(val label: String, val iconRes: Int, val color: Int) {
    Generators("Generators", android.R.drawable.ic_menu_add, 0xFF4CAF50),
    Filters("Filters", android.R.drawable.ic_menu_edit, 0xFF2196F3),
    Color("Color", android.R.drawable.ic_menu_gallery, 0xFFF44336),
    Blur("Blur", android.R.drawable.ic_menu_rotate, 0xFF9C27B0),
    Transform("Transform", android.R.drawable.ic_menu_crop, 0xFFFF9800),
    Composite("Composite", android.R.drawable.ic_menu_agenda, 0xFF3F51B5),
    Channel("Channel", android.R.drawable.ic_menu_slideshow, 0xFF00BCD4),
    Audio("Audio", android.R.drawable.ic_media_play, 0xFFE91E63)
}

@Composable
fun ToolCategoryItem(category: ToolCategory, onClick: () -> Unit) {
    androidx.compose.material3.TextButton(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth().height(40.dp).padding(horizontal = 12.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Start
        ) {
            Icon(
                painter = painterResource(id = category.iconRes),
                contentDescription = "",
                tint = Color(category.color)
            )
            androidx.compose.foundation.layout.Box(modifier = Modifier.width(12.dp))
            Text(text = category.label, color = Color.White, fontSize = 13.sp)
        }
    }
}

@Composable
fun FusionNodeCanvas(state: EditorState) {
    Box(
        modifier = Modifier
            .fillMaxSize()
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
fun FusionRightSidebar(
    state: EditorState,
    showInspector: Boolean,
    showKeyframes: Boolean,
    showSpline: Boolean
) {
    Card(
        modifier = Modifier
            .width(320.dp)
            .fillMaxHeight()
            .background(Color(0xFF121212))
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Tabs
            Row(
                modifier = Modifier.fillMaxWidth().height(40.dp).background(Color(0xFF0D0D0D))
            ) {
                listOf("Inspector", "Keyframes", "Spline").forEachIndexed { index, title ->
                    val isSelected = when (index) {
                        0 -> showInspector
                        1 -> showKeyframes
                        2 -> showSpline
                        else -> false
                    }
                    androidx.compose.material3.TextButton(
                        onClick = { /* handled by parent */ },
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(40.dp)
                    ) {
                        Text(
                            text = title,
                            color = if (isSelected) Color.Cyan else Color.White.copy(alpha = 0.7f),
                            fontSize = 12.sp,
                            fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Normal
                        )
                    }
                }
            }
            
            Divider(color = Color.White.copy(alpha = 0.1f))
            
            // Content
            if (showInspector) {
                FusionInspectorPanel(state)
            } else if (showKeyframes) {
                FusionKeyframeEditor(state)
            } else if (showSpline) {
                FusionSplineEditor(state)
            }
        }
    }
}

@Composable
fun FusionInspectorPanel(state: EditorState) {
    val selectedNode = state.selectedNodeId?.let { state.nodes.value[it] }
    
    androidx.compose.foundation.layout.Box(modifier = Modifier.fillMaxSize().padding(16.dp)) {
        if (selectedNode == null) {
            CenteredText("Select a node")
        } else {
            androidx.compose.foundation.lazy.LazyColumn(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
            ) {
                item {
                    Text(text = selectedNode.name, color = Color.White, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                    Text(text = selectedNode.type.label, color = Color(selectedNode.type.color), fontSize = 12.sp)
                }
                
                item {
                    Divider(color = Color.White.copy(alpha = 0.2f))
                    Text(text = "Controls", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                }
                
                selectedNode.uniforms.forEach { (name, value) ->
                    item {
                        FusionUniformField(
                            label = name,
                            value = value,
                            onValueChange = { newValue ->
                                state.updateNodeUniform(selectedNode.id, name, newValue)
                            }
                        )
                    }
                }
                
                if (selectedNode.animatedUniforms.isNotEmpty()) {
                    item {
                        Divider(color = Color.White.copy(alpha = 0.2f))
                        Text(text = "Animated", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                    }
                    
                    selectedNode.animatedUniforms.forEach { (name, track) ->
                        item {
                            FusionAnimatedField(state = state, nodeId = selectedNode.id, uniformName = name, track = track)
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun FusionUniformField(
    label: String,
    value: Float,
    onValueChange: (Float) -> Unit
) {
    var textValue by remember { mutableStateOf(value.toString()) }
    
    Column(modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        Text(text = label, color = Color.White.copy(alpha = 0.8f), fontSize = 12.sp)
        androidx.compose.material3.FilledTextField(
            value = textValue,
            onValueChange = { 
                textValue = it
                it.toFloatOrNull()?.let { onValueChange(it) }
            },
            modifier = Modifier.fillMaxWidth(),
            singleLine = true,
            textStyle = androidx.compose.ui.text.TextStyle(color = Color.White, fontSize = 12.sp),
            colors = androidx.compose.material3.TextFieldDefaults.filledTextFieldColors(
                containerColor = Color(0xFF2D2D2D),
                focusedContainerColor = Color(0xFF3D3D3D),
                textColor = Color.White,
                placeholderColor = Color.White.copy(alpha = 0.4f)
            )
        )
    }
}

@Composable
fun FusionAnimatedField(
    state: EditorState,
    nodeId: String,
    uniformName: String,
    track: EditorState.KeyframeTrack
) {
    Column(modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
        ) {
            Text(text = uniformName, color = Color.White.copy(alpha = 0.8f), fontSize = 12.sp)
            Text(text = "${track.keyframes.size} keys", color = Color.Green, fontSize = 10.sp)
        }
        
        track.keyframes.forEachIndexed { index, kf ->
            Row(
                modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
            ) {
                Text(text = "${formatTimecode(kf.time)}: ${kf.value}", color = Color.White.copy(alpha = 0.7f), fontSize = 11.sp)
                Text(text = kf.interpolation.name, color = Color.White.copy(alpha = 0.5f), fontSize = 10.sp)
            }
        }
    }
}

@Composable
fun FusionKeyframeEditor(state: EditorState) {
    val target = state.keyframeEditorTarget
    
    if (target == null) {
        CenteredText("Select an animated property in Inspector")
    } else {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            Text(text = "${target.nodeId} > ${target.uniformName}", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
            
            // Curve view
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color(0xFF0D0D0D))
                    .pointerInput(Unit) {
                        detectTapGestures(
                            onTap = { offset ->
                                // Add keyframe
                            }
                        )
                    }
            ) {
                // Grid and curve drawing would go here
                CenteredText("Curve Editor - ${target.track.keyframes.size} keyframes")
            }
        }
    }
}

@Composable
fun FusionSplineEditor(state: EditorState) {
    CenteredText("Spline Editor - Bezier handles for keyframe interpolation")
}

@Composable
fun FusionKeyframeTimeline(state: EditorState) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(200.dp)
            .background(Color(0xFF121212))
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            Text(text = "Keyframe Timeline", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
            
            // Would show keyframe tracks here
            CenteredText("Keyframe tracks for selected node")
        }
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