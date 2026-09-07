package com.vfxengine.app.ui.text

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Card
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.vfxengine.app.ui.common.EditorState

/**
 * Text Layers UI - manage text-based nodes on the timeline/graph.
 */
@Composable
fun TextLayers(
    state: EditorState,
    modifier: Modifier = Modifier
) {
    val textNodes = state.nodes.value.values.filter { node ->
        node.type == EditorState.NodeType.TextSource ||
        node.type == EditorState.NodeType.TextAnimator ||
        node.type == EditorState.NodeType.TextPath ||
        node.type == EditorState.NodeType.Typewriter
    }

    var selectedTextNodeId by remember { mutableStateOf<String?>(null) }
    val selectedNode = selectedTextNodeId?.let { state.nodes.value[it] }

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(Color(0xFF0F0F0F))
            .padding(16.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(text = "Text Layers", color = Color.White, fontSize = 18.sp, fontWeight = FontWeight.Bold)
            IconButton(onClick = { /* TODO: add text layer */ }) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_add),
                    contentDescription = "Add Text",
                    tint = Color.Cyan
                )
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        Row(modifier = Modifier.fillMaxSize()) {
            LazyColumn(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxHeight(),
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
            ) {
                item {
                    Box(
                        modifier = Modifier.fillMaxWidth(),
                        contentAlignment = Alignment.Center
                    ) {
                        if (textNodes.isEmpty()) {
                            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                                Icon(
                                    painter = painterResource(id = android.R.drawable.ic_menu_edit),
                                    contentDescription = null,
                                    tint = Color.White.copy(alpha = 0.3f),
                                    modifier = Modifier.size(48.dp)
                                )
                                Spacer(modifier = Modifier.height(16.dp))
                                Text(text = "No text layers", color = Color.White.copy(alpha = 0.5f), fontSize = 14.sp)
                            }
                        }
                    }
                }
                items(textNodes) { node ->
                    TextLayerRow(
                        node = node,
                        isSelected = selectedTextNodeId == node.id,
                        onSelect = { selectedTextNodeId = it }
                    )
                }
            }

            Spacer(modifier = Modifier.width(16.dp))

            if (selectedNode != null) {
                Card(
                    modifier = Modifier
                        .width(320.dp)
                        .fillMaxHeight()
                        .background(Color(0xFF1E1E1E))
                ) {
                    TextLayerEditor(
                        node = selectedNode,
                        onUpdateText = { state.nodes.value[selectedNode.id]?.textContent = it },
                        onUpdateFont = { state.nodes.value[selectedNode.id]?.fontPath = it },
                        onUpdateFontSize = { state.nodes.value[selectedNode.id]?.fontSize = it },
                        onUpdateAlignment = { state.nodes.value[selectedNode.id]?.alignment = it }
                    )
                }
            }
        }
    }
}

@Composable
fun TextLayerRow(
    node: com.vfxengine.app.ui.common.EditorState.Node,
    isSelected: Boolean,
    onSelect: (String) -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .background(if (isSelected) Color(0xFF1A3A4A) else Color(0xFF1E1E1E)),
        onClick = { onSelect(node.id) }
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = node.name,
                    color = Color.White,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Medium
                )
                Text(
                    text = node.type.label,
                    color = Color.Cyan,
                    fontSize = 11.sp
                )
            }

            Spacer(modifier = Modifier.height(8.dp))

            Text(
                text = node.textContent.ifBlank { "Empty text" },
                color = Color.White.copy(alpha = 0.7f),
                fontSize = 12.sp,
                maxLines = 2
            )
        }
    }
}

@Composable
fun TextLayerEditor(
    node: com.vfxengine.app.ui.common.EditorState.Node,
    onUpdateText: (String) -> Unit,
    onUpdateFont: (String) -> Unit,
    onUpdateFontSize: (Float) -> Unit,
    onUpdateAlignment: (Int) -> Unit
) {
    var text by remember { mutableStateOf(node.textContent) }
    var fontSize by remember { mutableStateOf(node.fontSize) }
    var alignment by remember { mutableStateOf(node.alignment) }

    Column(modifier = Modifier.padding(16.dp)) {
        Text(text = "Edit Text Layer", color = Color.White, fontSize = 16.sp, fontWeight = FontWeight.Bold)
        Spacer(modifier = Modifier.height(16.dp))

        TextField(
            value = text,
            onValueChange = {
                text = it
                onUpdateText(it)
            },
            modifier = Modifier.fillMaxWidth(),
            label = { Text("Text Content", color = Color.White.copy(alpha = 0.7f)) },
            colors = TextFieldDefaults.colors(
                focusedTextColor = Color.White,
                unfocusedTextColor = Color.White,
                focusedContainerColor = Color(0xFF121212),
                unfocusedContainerColor = Color(0xFF121212)
            )
        )

        Spacer(modifier = Modifier.height(16.dp))

        Text(text = "Font Size", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
        Slider(
            value = fontSize,
            onValueChange = {
                fontSize = it
                onUpdateFontSize(it)
            },
            valueRange = 8f..200f,
            colors = androidx.compose.material3.SliderDefaults.colors(
                thumbColor = Color.Cyan,
                activeTrackColor = Color.Cyan
            )
        )
        Text(text = "${fontSize.toInt()}px", color = Color.Cyan, fontSize = 12.sp)

        Spacer(modifier = Modifier.height(16.dp))

        Text(text = "Alignment", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
        ) {
            listOf(
                0 to "Left",
                1 to "Center",
                2 to "Right"
            ).forEach { (align, label) ->
                androidx.compose.material3.TextButton(
                    onClick = {
                        alignment = align
                        onUpdateAlignment(align)
                    },
                    modifier = Modifier.weight(1f),
                    colors = androidx.compose.material3.ButtonDefaults.textButtonColors(
                        containerColor = if (alignment == align) Color.Cyan.copy(alpha = 0.2f) else Color(0xFF1E1E1E)
                    )
                ) {
                    Text(
                        text = label,
                        color = if (alignment == align) Color.Cyan else Color.White,
                        fontSize = 12.sp
                    )
                }
            }
        }
    }
}

