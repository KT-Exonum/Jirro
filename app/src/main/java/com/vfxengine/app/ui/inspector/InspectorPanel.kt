package com.vfxengine.app.ui.inspector

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Card
import androidx.compose.material3.Divider
import androidx.compose.material3.FilledTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt
import com.vfxengine.app.ui.common.EditorState

/**
 * Inspector panel showing properties of selected node
 */
@Composable
fun InspectorPanel(state: EditorState) {
    val selectedNode = state.selectedNodeId?.let { state.nodes.value[it] }
    
    Box(
        modifier = Modifier
            .width(280.dp)
            .fillMaxHeight()
            .background(Color(0xFF1E1E1E))
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            if (selectedNode == null) {
                Text(
                    text = "Select a node to inspect",
                    color = Color.White.copy(alpha = 0.5f),
                    fontSize = 14.sp
                )
            } else {
                // Node header
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
                ) {
                    Text(
                        text = selectedNode.name,
                        color = Color.White,
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Bold
                    )
                    Text(
                        text = selectedNode.type.label,
                        color = Color(selectedNode.type.color),
                        fontSize = 12.sp
                    )
                }
                
                Divider(color = Color.White.copy(alpha = 0.2f), modifier = Modifier.padding(vertical = 8.dp))
                
                // Static uniforms
                Text(text = "Properties", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))
                
                selectedNode.uniforms.forEach { (name, value) ->
                    UniformField(
                        label = name,
                        value = value,
                        onValueChange = { newValue ->
                            state.updateNodeUniform(selectedNode.id, name, newValue)
                        }
                    )
                }
                
                Divider(color = Color.White.copy(alpha = 0.2f), modifier = Modifier.padding(vertical = 16.dp))
                
                // Animated uniforms
                if (selectedNode.animatedUniforms.isNotEmpty()) {
                    Text(text = "Animated Properties", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                    androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))
                    
                    selectedNode.animatedUniforms.forEach { (name, track) ->
                        androidx.compose.material3.Card(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(vertical = 4.dp),
                            onClick = {
                                state.keyframeEditorTarget = EditorState.KeyframeTarget(
                                    nodeId = selectedNode.id,
                                    uniformName = name,
                                    track = track
                                )
                            }
                        ) {
                            Column(modifier = Modifier.fillMaxWidth().padding(12.dp)) {
                                Row(
                                    modifier = Modifier.fillMaxWidth(),
                                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
                                ) {
                                    Text(text = name, color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Medium)
                                    Text(
                                        text = "${track.keyframes.size} KFs",
                                        color = Color(0xFF00E5FF),
                                        fontSize = 10.sp
                                    )
                                }
                                androidx.compose.foundation.layout.Box(modifier = Modifier.height(4.dp))
                                track.keyframes.forEachIndexed { index, kf ->
                                    Row(
                                        modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
                                        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
                                    ) {
                                        Text(
                                            text = "${formatTime(kf.time)}: ${String.format("%.3f", kf.value)}",
                                            color = Color.White.copy(alpha = 0.7f),
                                            fontSize = 11.sp
                                        )
                                        Text(
                                            text = kf.interpolation.name,
                                            color = Color.White.copy(alpha = 0.5f),
                                            fontSize = 10.sp
                                        )
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun UniformField(
    label: String,
    value: Float,
    onValueChange: (Float) -> Unit
) {
    var textValue by remember { mutableStateOf(value.toString()) }
    
    Column(modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        Text(text = label, color = Color.White.copy(alpha = 0.8f), fontSize = 12.sp)
        FilledTextField(
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

private fun formatTime(seconds: Double): String {
    val totalSeconds = seconds.roundToInt()
    val minutes = totalSeconds / 60
    val secs = totalSeconds % 60
    val frames = ((seconds - totalSeconds) * 30).roundToInt()
    return String.format("%02d:%02d.%02d", minutes, secs, frames)
}