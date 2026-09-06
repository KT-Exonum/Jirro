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
        val animatedUniforms: MutableMap<String, KeyframeTrack> = mutableMapOf(),
        var parentId: String? = null,
        var groupId: String? = null,
        var isGroupRoot: Boolean = false,
        var transform: Transform = Transform(),
        var expressions: MutableMap<String, Expression> = mutableMapOf(),
        // VectorSource
        var svgPathData: String = "",
        // TextSource
        var textContent: String = "",
        var fontPath: String = "",
        var fontSize: Float = 48.0f,
        var lineHeight: Float = 1.2f,
        var alignment: Int = 0,
        // StrokeSource
        var strokePoints: MutableList<Float> = mutableListOf(),
        var strokeWidth: Float = 2.0f,
        var strokeColor: Int = 0xFFFFFFFF,
        // Onion skinning (Output node)
        var onionSkinEnabled: Boolean = false,
        var onionSkinFramesBefore: Int = 2,
        var onionSkinFramesAfter: Int = 2,
        var onionSkinOpacityBefore: Float = 0.3f,
        var onionSkinOpacityAfter: Float = 0.3f,
    ) {
        fun copyWithChanges(block: Node.() -> Unit): Node {
            block()
            return this
        }
    }

    data class Transform(
        var positionX: Float = 0.0f,
        var positionY: Float = 0.0f,
        var rotation: Float = 0.0f,
        var scaleX: Float = 1.0f,
        var scaleY: Float = 1.0f,
        var anchorX: Float = 0.5f,
        var anchorY: Float = 0.5f,
    )

    data class Expression(
        var script: String = "",
        val dependencies: MutableList<String> = mutableListOf(),
    )

    enum class NodeType(val label: String, val color: Int, val kind: Int) {
        VideoSource("Video", 0xFF2196F3.toInt(), 0),
        ImageSource("Image", 0xFF4CAF50.toInt(), 1),
        AudioSource("Audio", 0xFFE91E63.toInt(), 2),
        Shader("Shader", 0xFF9C27B0.toInt(), 3),
        Blend("Blend", 0xFFFF9800.toInt(), 4),
        ColorCorrection("Color", 0xFFF44336.toInt(), 5),
        Blur("Blur", 0xFF607D8B.toInt(), 6),
        Mask("Mask", 0xFF795548.toInt(), 7),
        Composite("Composite", 0xFF3F51B5.toInt(), 8),
        Output("Output", 0xFF000000.toInt(), 9),
        Adjustment("Adjustment", 0xFF00BCD4.toInt(), 10),
        Null("Null", 0xFF9E9E9E.toInt(), 11),
        Group("Group", 0xFF8BC34A.toInt(), 12),
        VectorSource("Vector", 0xFF673AB7.toInt(), 13),
        TextSource("Text", 0xFF795548.toInt(), 14),
        StrokeSource("Stroke", 0xFF009688.toInt(), 15),
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

    // Node groups
    data class NodeGroup(
        val groupId: String,
        var name: String,
        val memberNodeIds: MutableList<String> = mutableListOf(),
        val exposedInputs: MutableList<Port> = mutableListOf(),
        val exposedOutputs: MutableList<Port> = mutableListOf(),
        var isCollapsed: Boolean = true,
    )

    val nodes = remember { mutableStateOf(mutableMapOf<String, Node>()) }
    val connections = remember { mutableStateOf(mutableListOf<Connection>()) }
    val groups = remember { mutableStateOf(mutableMapOf<String, NodeGroup>()) }
    var selectedNodeId by remember { mutableStateOf<String?>(null) }
    var selectedNodeIds by remember { mutableStateOf(mutableSetOf<String>()) }
    var selectedGroupIds by remember { mutableStateOf(mutableSetOf<String>()) }

    // Drag state for node editor
    var dragState by remember { mutableStateOf<DragState?>(null) }

    sealed class DragState {
        data class MovingNode(val nodeId: String, val startX: Float, val startY: Float) : DragState()
        data class MovingNodes(val nodeIds: Set<String>, val startX: Float, val startY: Float) : DragState()
        data class Connecting(val fromNodeId: String, val fromPort: String, val currentX: Float, val currentY: Float) : DragState()
        data class Panning(val startX: Float, val startY: Float, val offsetX: Float, val offsetY: Float) : DragState()
        data class MarqueeSelect(val startX: Float, val startY: Float, val currentX: Float, val currentY: Float) : DragState()
    }

    // Node editor pan/zoom
    var nodePanX by remember { mutableStateOf(0f) }
    var nodePanY by remember { mutableStateOf(0f) }
    var nodeZoom by remember { mutableStateOf(1f) }

    // Clips on timeline
    enum class ClipType { Video, Audio, Image, Vector, Text, Stroke }

    data class Clip(
        val id: String,
        val nodeId: String, // references a Source node
        var type: ClipType = ClipType.Video,
        var timelineStart: Double = 0.0,
        var sourceIn: Double = 0.0,
        var sourceOut: Double = 0.0,
        var speed: Double = 1.0,
        var layer: Int = 0,
        var enabled: Boolean = true,
        var locked: Boolean = false,
        val color: Int = 0xFF2196F3
    )

    val clips = remember { mutableStateOf(mutableListOf<Clip>()) }

    // Audio clip data
    data class AudioClip(
        val id: String,
        val clipId: String,
        var volume: Float = 1.0f,
        var pan: Float = 0.0f,
        var mute: Boolean = false,
        var solo: Boolean = false,
    )

    val audioClips = remember { mutableStateOf(mutableMapOf<String, AudioClip>()) }

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
        var outTangent: Float = 0f,
        // Custom graph support
        var customCurvePoints: MutableList<Float> = mutableListOf(), // for custom interpolation
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
                InterpolationType.Custom -> evaluateCustom(prev, next, t)
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

        private fun evaluateCustom(prev: Keyframe, next: Keyframe, t: Double): Float {
            // Custom curve evaluation using stored control points
            // For now fall back to linear
            prev.value + (next.value - prev.value) * t.toFloat()
        }
    }

    // Undo/Redo
    sealed class HistoryAction {
        data class AddNode(val nodeId: String, val node: Node) : HistoryAction()
        data class RemoveNode(val nodeId: String, val node: Node) : HistoryAction()
        data class UpdateNode(val nodeId: String, val propertyName: String, val oldValue: Any, val newValue: Any) : HistoryAction()
        data class AddConnection(val connection: Connection) : HistoryAction()
        data class RemoveConnection(val connection: Connection) : HistoryAction()
        data class AddClip(val clip: Clip) : HistoryAction()
        data class RemoveClip(val clip: Clip) : HistoryAction()
        data class UpdateClip(val clipId: String, val propertyName: String, val oldValue: Any, val newValue: Any) : HistoryAction()
        data class CreateGroup(val group: NodeGroup) : HistoryAction()
        data class RemoveGroup(val group: NodeGroup) : HistoryAction()
        data class MultiAction(val actions: List<HistoryAction>) : HistoryAction()
    }

    val history = remember { mutableStateOf(mutableListOf<HistoryAction>()) }
    var historyIndex by remember { mutableStateOf(0) }
    val maxHistorySize = 100

    fun recordAction(action: HistoryAction) {
        // Truncate future history if we're not at the end
        if (historyIndex < history.value.size) {
            history.value = history.value.subList(0, historyIndex).toMutableList()
        }
        history.value.add(action)
        if (history.value.size > maxHistorySize) {
            history.value.removeAt(0)
        } else {
            historyIndex = history.value.size
        }
    }

    fun canUndo(): Boolean = historyIndex > 0
    fun canRedo(): Boolean = historyIndex < history.value.size

    fun undo() {
        if (!canUndo()) return
        historyIndex--
        val action = history.value[historyIndex]
        revertAction(action)
    }

    fun redo() {
        if (!canRedo()) return
        val action = history.value[historyIndex]
        applyAction(action)
        historyIndex++
    }

    private fun applyAction(action: HistoryAction) {
        when (action) {
            is HistoryAction.AddNode -> {
                nodes.value[action.nodeId] = action.node
            }
            is HistoryAction.RemoveNode -> {
                nodes.value.remove(action.nodeId)
                connections.value.removeAll { it.fromNodeId == action.nodeId || it.toNodeId == action.nodeId }
            }
            is HistoryAction.UpdateNode -> {
                val node = nodes.value[action.nodeId] ?: return
                when (action.propertyName) {
                    "name" -> node.name = action.newValue as String
                    "x" -> node.x = action.newValue as Float
                    "y" -> node.y = action.newValue as Float
                    // Add more properties as needed
                }
            }
            is HistoryAction.AddConnection -> connections.value.add(action.connection)
            is HistoryAction.RemoveConnection -> connections.value.remove(action.connection)
            is HistoryAction.AddClip -> clips.value.add(action.clip)
            is HistoryAction.RemoveClip -> clips.value.remove(action.clip)
            is HistoryAction.UpdateClip -> {
                val clip = clips.value.firstOrNull { it.id == action.clipId } ?: return
                when (action.propertyName) {
                    "timelineStart" -> clip.timelineStart = action.newValue as Double
                    "sourceIn" -> clip.sourceIn = action.newValue as Double
                    "sourceOut" -> clip.sourceOut = action.newValue as Double
                    "speed" -> clip.speed = action.newValue as Double
                    "layer" -> clip.layer = action.newValue as Int
                    "enabled" -> clip.enabled = action.newValue as Boolean
                    "locked" -> clip.locked = action.newValue as Boolean
                }
            }
            is HistoryAction.CreateGroup -> groups.value[action.group.groupId] = action.group
            is HistoryAction.RemoveGroup -> groups.value.remove(action.group.groupId)
            is HistoryAction.MultiAction -> action.actions.forEach { applyAction(it) }
        }
    }

    private fun revertAction(action: HistoryAction) {
        when (action) {
            is HistoryAction.AddNode -> {
                nodes.value.remove(action.nodeId)
                connections.value.removeAll { it.fromNodeId == action.nodeId || it.toNodeId == action.nodeId }
            }
            is HistoryAction.RemoveNode -> {
                nodes.value[action.nodeId] = action.node
            }
            is HistoryAction.UpdateNode -> {
                val node = nodes.value[action.nodeId] ?: return
                when (action.propertyName) {
                    "name" -> node.name = action.oldValue as String
                    "x" -> node.x = action.oldValue as Float
                    "y" -> node.y = action.oldValue as Float
                }
            }
            is HistoryAction.AddConnection -> connections.value.remove(action.connection)
            is HistoryAction.RemoveConnection -> connections.value.add(action.connection)
            is HistoryAction.AddClip -> clips.value.remove(action.clip)
            is HistoryAction.RemoveClip -> clips.value.add(action.clip)
            is HistoryAction.UpdateClip -> {
                val clip = clips.value.firstOrNull { it.id == action.clipId } ?: return
                when (action.propertyName) {
                    "timelineStart" -> clip.timelineStart = action.oldValue as Double
                    "sourceIn" -> clip.sourceIn = action.oldValue as Double
                    "sourceOut" -> clip.sourceOut = action.oldValue as Double
                    "speed" -> clip.speed = action.oldValue as Double
                    "layer" -> clip.layer = action.oldValue as Int
                    "enabled" -> clip.enabled = action.oldValue as Boolean
                    "locked" -> clip.locked = action.oldValue as Boolean
                }
            }
            is HistoryAction.CreateGroup -> groups.value.remove(action.group.groupId)
            is HistoryAction.RemoveGroup -> groups.value[action.group.groupId] = action.group
            is HistoryAction.MultiAction -> action.actions.forEach { revertAction(it) }
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
            NodeType.VideoSource, NodeType.ImageSource, NodeType.AudioSource -> {
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
            NodeType.Adjustment -> {
                node.inputs.add(Port("input", Port.PortType.Input))
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            NodeType.Null -> {
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            NodeType.VectorSource -> {
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            NodeType.TextSource -> {
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            NodeType.StrokeSource -> {
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            NodeType.Group -> {
                // Group ports are dynamic based on exposed inputs/outputs
            }
        }
        nodes.value[id] = node
        
        // Record for undo
        recordAction(HistoryAction.AddNode(id, node))
        
        return node
    }

    fun removeNode(id: String) {
        val node = nodes.value[id] ?: return
        nodes.value.remove(id)
        connections.value.removeAll { it.fromNodeId == id || it.toNodeId == id }
        if (selectedNodeId == id) selectedNodeId = null
        selectedNodeIds.remove(id)
        
        // Record for undo
        recordAction(HistoryAction.RemoveNode(id, node))
    }

    fun connect(fromNodeId: String, fromPort: String, toNodeId: String, toPort: String) {
        // Prevent self-connections and cycles (basic check)
        if (fromNodeId == toNodeId) return
        val connection = Connection(fromNodeId, fromPort, toNodeId, toPort)
        connections.value.add(connection)
        
        // Record for undo
        recordAction(HistoryAction.AddConnection(connection))
    }

    fun disconnect(connection: Connection) {
        connections.value.remove(connection)
        recordAction(HistoryAction.RemoveConnection(connection))
    }

    fun updateNodeUniform(nodeId: String, uniformName: String, value: Float) {
        val node = nodes.value[nodeId] ?: return
        val oldValue = node.uniforms[uniformName] ?: 0f
        node.uniforms[uniformName] = value
        nativeEngine.updateUniform(nodeId, uniformName, value)
        
        // Record for undo
        recordAction(HistoryAction.UpdateNode(nodeId, uniformName, oldValue, value))
    }

    fun setNodeTransform(nodeId: String, propertyName: String, value: Float) {
        val node = nodes.value[nodeId] ?: return
        val oldValue = when (propertyName) {
            "positionX" -> node.transform.positionX
            "positionY" -> node.transform.positionY
            "rotation" -> node.transform.rotation
            "scaleX" -> node.transform.scaleX
            "scaleY" -> node.transform.scaleY
            else -> 0f
        }
        when (propertyName) {
            "positionX" -> node.transform.positionX = value
            "positionY" -> node.transform.positionY = value
            "rotation" -> node.transform.rotation = value
            "scaleX" -> node.transform.scaleX = value
            "scaleY" -> node.transform.scaleY = value
        }
        // Also update uniforms for animation
        node.uniforms[propertyName] = value
        nativeEngine.updateUniform(nodeId, propertyName, value)
        recordAction(HistoryAction.UpdateNode(nodeId, propertyName, oldValue, value))
    }

    fun setNodeParent(nodeId: String, parentId: String?) {
        val node = nodes.value[nodeId] ?: return
        val oldParent = node.parentId
        node.parentId = parentId
        recordAction(HistoryAction.UpdateNode(nodeId, "parentId", oldValue = oldParent ?: "", newValue = parentId ?: ""))
    }

    fun createGroup(groupId: String, name: String, memberIds: List<String>) {
        val group = NodeGroup(groupId = groupId, name = name, memberNodeIds = memberIds.toMutableList())
        groups.value[groupId] = group
        memberIds.forEach { memberId ->
            nodes.value[memberId]?.groupId = groupId
        }
        recordAction(HistoryAction.CreateGroup(group))
    }

    fun removeGroup(groupId: String) {
        val group = groups.value[groupId] ?: return
        group.memberNodeIds.forEach { memberId ->
            nodes.value[memberId]?.groupId = null
        }
        groups.value.remove(groupId)
        recordAction(HistoryAction.RemoveGroup(group))
    }

    fun addClip(clip: Clip) {
        clips.value.add(clip)
        recordAction(HistoryAction.AddClip(clip))
    }

    fun removeClip(clipId: String) {
        val clip = clips.value.firstOrNull { it.id == clipId } ?: return
        clips.value.remove(clip)
        recordAction(HistoryAction.RemoveClip(clip))
    }

    fun updateClip(clipId: String, propertyName: String, newValue: Any) {
        val clip = clips.value.firstOrNull { it.id == clipId } ?: return
        val oldValue = when (propertyName) {
            "timelineStart" -> clip.timelineStart
            "sourceIn" -> clip.sourceIn
            "sourceOut" -> clip.sourceOut
            "speed" -> clip.speed
            "layer" -> clip.layer
            "enabled" -> clip.enabled
            "locked" -> clip.locked
            else -> return
        }
        when (propertyName) {
            "timelineStart" -> clip.timelineStart = newValue as Double
            "sourceIn" -> clip.sourceIn = newValue as Double
            "sourceOut" -> clip.sourceOut = newValue as Double
            "speed" -> clip.speed = newValue as Double
            "layer" -> clip.layer = newValue as Int
            "enabled" -> clip.enabled = newValue as Boolean
            "locked" -> clip.locked = newValue as Boolean
        }
        recordAction(HistoryAction.UpdateClip(clipId, propertyName, oldValue, newValue))
    }

    fun addKeyframe(nodeId: String, uniformName: String, time: Double, value: Float) {
        val node = nodes.value[nodeId] ?: return
        val track = node.animatedUniforms.getOrPut(uniformName) { KeyframeTrack() }
        // Replace existing keyframe at same time or add new
        val existing = track.keyframes.firstOrNull { Math.abs(it.time - time) < 0.001 }
        if (existing != null) {
            val oldValue = existing.value
            existing.value = value
            recordAction(HistoryAction.UpdateNode(nodeId, "keyframe_$uniformName", oldValue, value))
        } else {
            track.keyframes.add(Keyframe(time, value))
            track.keyframes.sortBy { it.time }
        }
        // Also update static uniform as fallback
        node.uniforms[uniformName] = value
        nativeEngine.updateUniform(nodeId, uniformName, value)
    }

    fun addAudioClip(clipId: String) {
        val audioClip = AudioClip(id = "audio_$clipId", clipId = clipId)
        audioClips.value[clipId] = audioClip
    }

    fun updateAudioClip(clipId: String, propertyName: String, newValue: Any) {
        val audioClip = audioClips.value[clipId] ?: return
        when (propertyName) {
            "volume" -> audioClip.volume = newValue as Float
            "pan" -> audioClip.pan = newValue as Float
            "mute" -> audioClip.mute = newValue as Boolean
            "solo" -> audioClip.solo = newValue as Boolean
        }
    }

    fun loadMediaFiles() {
        // TODO: Implement media scanning from device storage
        // For now, add some placeholder
    }

    fun undo() {
        nativeEngine.undo()
    }

    fun redo() {
        nativeEngine.redo()
    }
}