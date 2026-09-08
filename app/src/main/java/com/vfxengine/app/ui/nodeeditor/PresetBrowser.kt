package com.vfxengine.app.ui.nodeeditor

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.material3.Card
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.vfxengine.app.ui.common.EditorState

/**
 * Preset browser for effects, transitions, and generators.
 */
@Composable
fun PresetBrowser(
    state: EditorState,
    modifier: Modifier = Modifier,
    categories: List<PresetCategory> = DefaultPresetCategories
) {
    var selectedCategory by remember { mutableStateOf(categories.firstOrNull()) }
    var searchQuery by remember { mutableStateOf("") }
    
    Box(modifier = modifier.background(Color(0xFF1E1E1E))) {
        Column(modifier = Modifier.fillMaxWidth().padding(8.dp)) {
            Text(
                text = "Presets",
                color = Color.White,
                fontSize = 16.sp,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.padding(bottom = 8.dp)
            )
            
            // Search
            androidx.compose.material3.OutlinedTextField(
                value = searchQuery,
                onValueChange = { searchQuery = it },
                label = { Text("Search presets", color = Color.Gray) },
                modifier = Modifier.fillMaxWidth().padding(bottom = 8.dp),
                colors = androidx.compose.material3.OutlinedTextFieldDefaults.colors(
                    focusedTextColor = Color.White,
                    unfocusedTextColor = Color.White
                )
            )
            
            // Category tabs
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(4.dp)
            ) {
                categories.forEach { category ->
                    val isSelected = category == selectedCategory
                    Card(
                        modifier = Modifier
                            .weight(1f)
                            .clickable { selectedCategory = category },
                        colors = androidx.compose.material3.CardDefaults.cardColors(
                            containerColor = if (isSelected) Color(0xFF3D3D3D) else Color(0xFF2D2D2D)
                        )
                    ) {
                        Text(
                            text = category.name,
                            color = if (isSelected) Color.White else Color.Gray,
                            fontSize = 12.sp,
                            modifier = Modifier.padding(vertical = 8.dp, horizontal = 4.dp),
                            fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Normal
                        )
                    }
                }
            }
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))
            
            // Preset grid
            val filteredPresets = categories
                .flatMap { it.presets }
                .filter { it.name.contains(searchQuery, ignoreCase = true) }
            
            LazyVerticalGrid(
                columns = GridCells.Fixed(2),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(filteredPresets) { preset ->
                    PresetCard(
                        preset = preset,
                        onClick = {
                            val node = state.addNode(preset.nodeType, 200f + state.nodes.value.size * 30f, 200f)
                            preset.apply(node)
                        }
                    )
                }
            }
        }
    }
}

@Composable
fun PresetCard(preset: Preset, onClick: () -> Unit) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() },
        colors = androidx.compose.material3.CardDefaults.cardColors(
            containerColor = Color(0xFF2D2D2D)
        )
    ) {
        Column(modifier = Modifier.padding(12.dp)) {
            Text(
                text = preset.name,
                color = Color.White,
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold
            )
            Text(
                text = preset.description,
                color = Color.Gray,
                fontSize = 12.sp,
                modifier = Modifier.padding(top = 4.dp)
            )
            if (preset.tags.isNotEmpty()) {
                Row(
                    modifier = Modifier.padding(top = 8.dp),
                    horizontalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    preset.tags.take(2).forEach { tag ->
                        Text(
                            text = tag,
                            color = Color(0xFF2196F3),
                            fontSize = 10.sp,
                            modifier = Modifier
                                .background(Color(0xFF2196F3).copy(alpha = 0.2f))
                                .padding(horizontal = 4.dp, vertical = 2.dp)
                        )
                    }
                }
            }
        }
    }
}

data class PresetCategory(
    val name: String,
    val presets: List<Preset>
)

data class Preset(
    val name: String,
    val description: String,
    val nodeType: EditorState.NodeType,
    val tags: List<String> = emptyList(),
    val apply: (EditorState.Node) -> Unit = {}
)

val DefaultPresetCategories = listOf(
    PresetCategory(
        name = "Blur",
        presets = listOf(
            Preset("Gaussian Blur", "Smooth gaussian blur", EditorState.NodeType.Blur, listOf("blur", "smooth")) { node ->
                node.uniforms["radius"] = 10.0f
                node.uniforms["quality"] = 3.0f
            },
            Preset("Motion Blur", "Directional motion blur", EditorState.NodeType.MotionBlur, listOf("motion", "direction")) { node ->
                node.uniforms["angle"] = 0.0f
                node.uniforms["distance"] = 20.0f
            }
        )
    ),
    PresetCategory(
        name = "Color",
        presets = listOf(
            Preset("Vintage", "Warm vintage color grade", EditorState.NodeType.ColorCorrection, listOf("warm", "retro")) { node ->
                node.uniforms["brightness"] = 1.1f
                node.uniforms["contrast"] = 0.9f
                node.uniforms["saturation"] = 0.8f
            },
            Preset("Cinematic", "Teal and orange cinematic grade", EditorState.NodeType.ColorCorrection, listOf("cinematic", "teal")) { node ->
                node.uniforms["contrast"] = 1.2f
                node.uniforms["saturation"] = 1.1f
            }
        )
    ),
    PresetCategory(
        name = "Audio",
        presets = listOf(
            Preset("Bass Pulse", "Bass-reactive scale pulse", EditorState.NodeType.AudioReactive, listOf("bass", "pulse")) { node ->
                node.uniforms["sensitivity"] = 2.0f
                node.uniforms["frequencyMin"] = 20.0f
                node.uniforms["frequencyMax"] = 200.0f
            },
            Preset("Beat Sync", "Beat-synced opacity", EditorState.NodeType.AudioWaveform, listOf("beat", "sync")) { node ->
                node.uniforms["useBeatDetection"] = 1.0f
                node.uniforms["beatThreshold"] = 0.5f
            }
        )
    ),
    PresetCategory(
        name = "Distort",
        presets = listOf(
            Preset("Glitch", "Digital glitch effect", EditorState.NodeType.Glitch, listOf("digital", "corruption")) { node ->
                node.uniforms["amount"] = 0.5f
                node.uniforms["speed"] = 10.0f
            },
            Preset("Wave", "Sine wave distortion", EditorState.NodeType.Wave, listOf("liquid", "organic")) { node ->
                node.uniforms["amplitude"] = 20.0f
                node.uniforms["frequency"] = 2.0f
                node.uniforms["speed"] = 1.0f
            }
        )
    )
)
