package com.vfxengine.app.ui.common

import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import kotlin.math.max

/**
 * Shared UI state for the editor. All timeline/node/keyframe state lives here
 * so multiple composables can observe and mutate it without prop-drilling.
 * The native engine is driven by JNI commands posted from this state.
 */
class EditorState(
    private val nativeEngine: com.vfxengine.app.NativeEngine
) {
    // Timeline
    var currentTimeSeconds by remember { mutableStateOf(0.0) }
    var durationSeconds by remember { mutableStateOf(10.0) }
    var isPlaying by remember { mutableStateOf(false) }
    var playbackSpeed by remember { mutableStateOf(1.0) }
    var isScrubbing by remember { mutableStateOf(false) }

    // Timeline zoom/pan
    var timeScale by remember { mutableStateOf(50.0) } // pixels per second
    var timeOffset by remember { mutableStateOf(0.0) }

    // Node graph
    data class Node(
        val id: String,
        val type: NodeType,
        var name: String,
        var x: Float,
        var y: Float,
        val inputs: MutableList<Port> = mutableListOf(),
        val outputs: MutableList<Port> = mutableListOf(),
        val uniforms: MutableMap<String, Float> = mutableMapOf(),
        val animatedUniforms: MutableMap<String, KeyframeTrack> = mutableMapOf()
    )

    enum class NodeType(val label: String, val color: Int) {
        VideoSource("Video", 0xFF2196F3.toInt()),
        ImageSource("Image", 0xFF4CAF50.toInt()),
        Shader("Shader", 0xFF9C27B0.toInt()),
        Blend("Blend", 0xFFFF9800.toInt()),
        ColorCorrection("Color", 0xFFF44336.toInt()),
        Blur("Blur", 0xFF607D8B.toInt()),
        Mask("Mask", 0xFF795548.toInt()),
        Composite("Composite", 0xFF3F51B5.toInt()),
        Output("Output", 0xFF000000.toInt())
    }

    data class Port(val name: String, val type: PortType) {
        enum class PortType { Input, Output }
    }

    data class Connection(
        val fromNodeId: String,
        val fromPort: String,
        val toNodeId: String,
        val toPort: String
    )

    val nodes = remember { mutableStateOf(mutableMapOf<String, Node>()) }
    val connections = remember { mutableStateOf(mutableListOf<Connection>()) }
    var selectedNodeId by remember { mutableStateOf<String?>(null) }

    // Drag state for node editor
    var dragState by remember { mutableStateOf<DragState?>(null) }

    sealed class DragState {
        data class MovingNode(val nodeId: String, val startX: Float, val startY: Float) : DragState()
        data class Connecting(val fromNodeId: String, val fromPort: String, val currentX: Float, val currentY: Float) : DragState()
        data class Panning(val startX: Float, val startY: Float, val offsetX: Float, val offsetY: Float) : DragState()
    }

    // Node editor pan/zoom
    var nodePanX by remember { mutableStateOf(0f) }
    var nodePanY by remember { mutableStateOf(0f) }
    var nodeZoom by remember { mutableStateOf(1f) }

    // Clips on timeline
    data class Clip(
        val id: String,
        val nodeId: String, // references a VideoSource node
        var timelineStart: Double,
        var sourceIn: Double,
        var sourceOut: Double,
        var speed: Double = 1.0,
        var layer: Int = 0,
        val color: Int = 0xFF2196F3
    )

    val clips = remember { mutableStateOf(mutableListOf<Clip>()) }

    // Keyframe editor
    var keyframeEditorTarget by remember { mutableStateOf<KeyframeTarget?>(null) }

    data class KeyframeTarget(
        val nodeId: String,
        val uniformName: String,
        val track: KeyframeTrack
    )

    // Media browser
    var mediaFiles by remember { mutableStateOf<List<MediaFile>>(emptyList()) }
    var selectedMediaFile by remember { mutableStateOf<MediaFile?>(null) }

    data class MediaFile(
        val path: String,
        val name: String,
        val duration: Double,
        val width: Int,
        val height: Int
    )

    // Keyframe track for UI
    data class Keyframe(
        var time: Double,
        var value: Float,
        var interpolation: InterpolationType = InterpolationType.Linear,
        var inTangent: Float = 0f,
        var outTangent: Float = 0f
    )

    enum class InterpolationType { Step, Linear, Bezier, Custom }

    data class KeyframeTrack(
        val keyframes: MutableList<Keyframe> = mutableListOf()
    ) {
        fun evaluate(time: Double): Float {
            if (keyframes.isEmpty()) return 0f
            if (keyframes.size == 1 || time <= keyframes.first().time) return keyframes.first().value
            if (time >= keyframes.last().time) return keyframes.last().value

            val next = keyframes.firstOrNull { it.time > time } ?: return keyframes.last().value
            val prev = keyframes.lastOrNull { it.time <= time } ?: return keyframes.first().value

            val span = next.time - prev.time
            val t = if (span > 0) (time - prev.time) / span else 0.0

            return when (prev.interpolation) {
                InterpolationType.Step -> prev.value
                InterpolationType.Linear -> prev.value + (next.value - prev.value) * t.toFloat()
                InterpolationType.Bezier -> evaluateBezier(prev, next, t)
                InterpolationType.Custom -> prev.value + (next.value - prev.value) * t.toFloat()
            }
        }

        private fun evaluateBezier(prev: Keyframe, next: Keyframe, t: Double): Float {
            val p0 = prev.value
            val p1 = prev.value + prev.outTangent
            val p2 = next.value + next.inTangent
            val p3 = next.value
            val u = 1.0 - t
            return (u*u*u*p0 + 3*u*u*t*p1 + 3*u*t*t*p2 + t*t*t*p3).toFloat()
        }
    }

    // Actions that send commands to native engine
    fun seek(seconds: Double) {
        currentTimeSeconds = max(0.0, seconds).coerceAtMost(durationSeconds)
        nativeEngine.seek(currentTimeSeconds)
    }

    fun setPlaying(playing: Boolean) {
        isPlaying = playing
        nativeEngine.setPlaying(playing)
    }

    fun setPlaybackSpeed(speed: Double) {
        playbackSpeed = speed
        nativeEngine.setPlaybackSpeed(speed)
    }

    fun setScrubbing(scrubbing: Boolean) {
        isScrubbing = scrubbing
        nativeEngine.setScrubbing(scrubbing)
        if (scrubbing) {
            isPlaying = false
            nativeEngine.setPlaying(false)
        }
    }

    fun addNode(type: NodeType, x: Float, y: Float): Node {
        val id = "node_${System.currentTimeMillis()}_${(0..999).random()}"
        val node = Node(
            id = id,
            type = type,
            name = "${type.label} ${nodes.value.size + 1}",
            x = x,
            y = y
        )
        // Add default ports based on type
        when (type) {
            NodeType.VideoSource, NodeType.ImageSource -> {
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            NodeType.Shader, NodeType.ColorCorrection, NodeType.Blur, NodeType.Mask -> {
                node.inputs.add(Port("input", Port.PortType.Input))
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            NodeType.Blend, NodeType.Composite -> {
                node.inputs.add(Port("base", Port.PortType.Input))
                node.inputs.add(Port("overlay", Port.PortType.Input))
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            NodeType.Output -> {
                node.inputs.add(Port("input", Port.PortType.Input))
            }
        }
        nodes.value[id] = node
        return node
    }

    fun removeNode(id: String) {
        nodes.value.remove(id)
        connections.value.removeAll { it.fromNodeId == id || it.toNodeId == id }
        if (selectedNodeId == id) selectedNodeId = null
    }

    fun connect(fromNodeId: String, fromPort: String, toNodeId: String, toPort: String) {
        // Prevent self-connections and cycles (basic check)
        if (fromNodeId == toNodeId) return
        connections.value.add(Connection(fromNodeId, fromPort, toNodeId, toPort))
    }

    fun updateNodeUniform(nodeId: String, uniformName: String, value: Float) {
        nodes.value[nodeId]?.uniforms?.put(uniformName, value)
        nativeEngine.updateUniform(nodeId, uniformName, value)
    }

    fun addKeyframe(nodeId: String, uniformName: String, time: Double, value: Float) {
        val node = nodes.value[nodeId] ?: return
        val track = node.animatedUniforms.getOrPut(uniformName) { KeyframeTrack() }
        // Replace existing keyframe at same time or add new
        val existing = track.keyframes.firstOrNull { Math.abs(it.time - time) < 0.001 }
        if (existing != null) {
            existing.value = value
        } else {
            track.keyframes.add(Keyframe(time, value))
            track.keyframes.sortBy { it.time }
        }
        // Also update static uniform as fallback
        node.uniforms[uniformName] = value
        nativeEngine.updateUniform(nodeId, uniformName, value)
    }

    fun loadMediaFiles() {
        // TODO: Implement media scanning from device storage
        // For now, add some placeholder
    }
}