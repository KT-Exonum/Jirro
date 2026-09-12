package com.vfxengine.app.ui.nodeeditor

import androidx.compose.foundation.Canvas
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
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.Divider
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
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.vfxengine.app.ui.common.EditorState

/**
 * Port on a node (input or output)
 */
@Composable
fun Port(
    state: EditorState,
    node: EditorState.Node,
    port: EditorState.Port,
    isOutput: Boolean,
    index: Int,
    onDragStart: (EditorState.DragState.Connecting) -> Unit
) {
    val isConnected = state.connections.value.any { c ->
        if (isOutput) c.fromNodeId == node.id && c.fromPort == port.name
        else c.toNodeId == node.id && c.toPort == port.name
    }

    Box(
        modifier = Modifier
            .size(12.dp)
            .background(if (isConnected) Color(node.type.color) else Color.Gray)
            .graphicsLayer {
                translationX = if (isOutput) nodeWidth(node) - 12f else 0f
            }
            .pointerInput(Unit) {
                detectDragGestures(
                    onDragStart = { onDragStart(EditorState.DragState.Connecting(node.id, port.name, 0f, 0f, isOutput)) },
                    onDrag = { change, dragAmount ->
                        // Update connection preview
                    },
                    onDragEnd = { }
                )
            }
    )
}

private fun nodeWidth(node: EditorState.Node): Float {
    return 180f // Fixed width for now
}

/**
 * Single node in the graph
 */
@Composable
fun NodeView(
    state: EditorState,
    node: EditorState.Node,
    onNodeClick: (String) -> Unit
) {
    val isSelected = state.selectedNodeId == node.id
    val maxPorts = maxOf(node.inputs.size, node.outputs.size)
    val nodeHeight = maxOf(80f, maxPorts * 36f + 40f)

    Box(
        modifier = Modifier
            .width(nodeWidth(node).dp)
            .height(nodeHeight.dp)
            .background(if (isSelected) Color(0xFF3D3D3D) else Color(0xFF2D2D2D))
            .graphicsLayer {
                translationX = node.x
                translationY = node.y
            }
            .pointerInput(node) {
                detectTapGestures(onTap = { onNodeClick(node.id) })
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
        Column(
            modifier = Modifier.fillMaxSize().padding(8.dp)
        ) {
            // Node header
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
            ) {
                Text(text = node.name, color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                Text(text = node.type.label, color = Color(node.type.color), fontSize = 10.sp)
            }
            
            // Ports
            val portCount = maxOf(node.inputs.size, node.outputs.size)
            repeat(portCount) { i ->
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
                ) {
                    // Input port
                    if (i < node.inputs.size) {
                        Port(state, node, node.inputs[i], false, i) { dragState ->
                            // Handle drag start
                        }
                    } else {
                        androidx.compose.foundation.layout.Box(modifier = Modifier.size(12.dp))
                    }
                    
                    // Output port
                    if (i < node.outputs.size) {
                        Port(state, node, node.outputs[i], true, i) { dragState ->
                            // Handle drag start
                        }
                    } else {
                        androidx.compose.foundation.layout.Box(modifier = Modifier.size(12.dp))
                    }
                }
            }
        }
    }
}

/**
 * Connection line between nodes
 */
@Composable
fun ConnectionLine(
    state: EditorState,
    connection: EditorState.Connection
) {
    val fromNode = state.nodes.value[connection.fromNodeId]
    val toNode = state.nodes.value[connection.toNodeId]
    
    fromNode?.let { from ->
        toNode?.let { to ->
            val fromX = from.x + nodeWidth(from)
            val fromY = from.y + 20 + from.outputs.indexOfFirst { it.name == connection.fromPort } * 36f
            val toX = to.x
            val toY = to.y + 20 + to.inputs.indexOfFirst { it.name == connection.toPort } * 36f
            
            // Draw bezier curve
            Canvas(
                modifier = Modifier.fillMaxSize()
            ) {
                val path = androidx.compose.ui.graphics.Path()
                path.moveTo(fromX, fromY)
                val ctrlX1 = fromX + 50
                val ctrlX2 = toX - 50
                path.cubicTo(ctrlX1, fromY, ctrlX2, toY, toX, toY)
                drawPath(
                    path = path,
                    color = Color.White.copy(alpha = 0.6f),
                    style = androidx.compose.ui.graphics.drawscope.Stroke(width = 2f)
                )
            }
        }
    }
}

/**
 * Main node editor canvas
 */
@Composable
fun NodeEditor(state: EditorState) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFF1A1A1A))
            .pointerInput(Unit) {
                detectDragGestures(
                    onDragStart = { },
                    onDrag = { change, dragAmount ->
                        // Pan canvas
                        state.nodePanX += dragAmount.x
                        state.nodePanY += dragAmount.y
                    },
                    onDragEnd = { }
                )
                detectTapGestures(
                    onTap = { offset ->
                        // Deselect on background tap
                        state.selectedNodeId = null
                    }
                )
            }
            .graphicsLayer {
                scaleX = state.nodeZoom
                scaleY = state.nodeZoom
                translationX = state.nodePanX
                translationY = state.nodePanY
            }
    ) {
        // Draw connections first (behind nodes)
        state.connections.value.forEach { conn ->
            ConnectionLine(state, conn)
        }
        
        // Draw nodes
        state.nodes.value.values.forEach { node ->
            NodeView(state, node) { nodeId ->
                state.selectedNodeId = nodeId
            }
        }
        
        // Draw connection preview during drag
        state.dragState?.let { drag ->
            when (drag) {
                is EditorState.DragState.Connecting -> {
                    val fromNode = state.nodes.value[drag.fromNodeId]
                    fromNode?.let { node ->
                        val fromX = node.x + nodeWidth(node) + state.nodePanX * state.nodeZoom
                        val fromY = node.y + 20 + node.outputs.indexOfFirst { it.name == drag.fromPort } * 36f * state.nodeZoom + state.nodePanY * state.nodeZoom
                        
                        Canvas(modifier = Modifier.fillMaxSize()) {
                            val path = androidx.compose.ui.graphics.Path()
                            path.moveTo(fromX, fromY)
                            val ctrlX1 = fromX + 50
                            val ctrlX2 = drag.currentX - 50
                            path.cubicTo(ctrlX1, fromY, ctrlX2, drag.currentY, drag.currentX, drag.currentY)
                            drawPath(
                                path = path,
                                color = Color.White.copy(alpha = 0.4f),
                                style = androidx.compose.ui.graphics.drawscope.Stroke(width = 2f, cap = androidx.compose.ui.graphics.StrokeCap.Round)
                            )
                        }
                    }
                }
                else -> {}
            }
        }
    }
}

/**
 * Toolbar for node editor
 */
@Composable
fun NodeEditorToolbar(state: EditorState) {
    Row(modifier = Modifier.padding(16.dp)) {
        EditorState.NodeType.values().forEach { type ->
            androidx.compose.material3.OutlinedButton(
                onClick = { 
                    val centerX = -state.nodePanX / state.nodeZoom + 400f / state.nodeZoom
                    val centerY = -state.nodePanY / state.nodeZoom + 300f / state.nodeZoom
                    state.addNode(type, centerX, centerY)
                },
                modifier = Modifier.padding(end = 8.dp)
            ) {
                Text(text = type.label, color = Color(type.color))
            }
        }
    }
}