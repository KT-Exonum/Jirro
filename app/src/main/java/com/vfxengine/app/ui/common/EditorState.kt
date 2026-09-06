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
        // Motion blur
        MotionBlur("Motion Blur", 0xFFE91E63.toInt(), 16),
        DirectionalBlur("Directional Blur", 0xFFE91E63.toInt(), 17),
        TransformBlur("Transform Blur", 0xFFE91E63.toInt(), 18),
        // Velocity/Time remap
        VelocityGraph("Velocity Graph", 0xFF00BCD4.toInt(), 19),
        TimeRemap("Time Remap", 0xFF00BCD4.toInt(), 20),
        OpticalFlow("Optical Flow", 0xFF00BCD4.toInt(), 21),
        // Masking/Rotoscoping
        BezierMask("Bezier Mask", 0xFF9C27B0.toInt(), 22),
        Rotoscoping("Rotoscoping", 0xFF9C27B0.toInt(), 23),
        RotoBrush("Roto Brush", 0xFF9C27B0.toInt(), 24),
        Tracker("Tracker", 0xFF9C27B0.toInt(), 25),
        // Particle system
        ParticleEmitter("Particle Emitter", 0xFFFF5722.toInt(), 26),
        ParticleForces("Particle Forces", 0xFFFF5722.toInt(), 27),
        ParticleRenderer("Particle Renderer", 0xFFFF5722.toInt(), 28),
        // Shape2D System
        ShapeRectangle("Rectangle", 0xFF4CAF50.toInt(), 29),
        ShapeEllipse("Ellipse", 0xFF4CAF50.toInt(), 30),
        ShapePolygon("Polygon", 0xFF4CAF50.toInt(), 31),
        ShapeStar("Star", 0xFF4CAF50.toInt(), 32),
        ShapePath("Path", 0xFF4CAF50.toInt(), 33),
        ShapeRender("Shape Render", 0xFF4CAF50.toInt(), 34),
        ShapeMerge("Shape Merge", 0xFF4CAF50.toInt(), 35),
        ShapeTransform("Shape Transform", 0xFF4CAF50.toInt(), 36),
        ShapeStroke("Shape Stroke", 0xFF4CAF50.toInt(), 37),
        ShapeFill("Shape Fill", 0xFF4CAF50.toInt(), 38),
        ShapeRepeater("Shape Repeater", 0xFF4CAF50.toInt(), 39),
        ShapeBoolean("Shape Boolean", 0xFF4CAF50.toInt(), 40),
        // 2.5D System (Z-axis for 2D planes)
        Transform3D("Transform 3D", 0xFF673AB7.toInt(), 41),
        Camera3D("Camera 3D", 0xFF673AB7.toInt(), 42),
        DepthOfField("Depth of Field", 0xFF673AB7.toInt(), 43),
        // Keying/Compositing
        ChromaKey("Chroma Key", 0xFFE91E63.toInt(), 44),
        // 3D Models
        MeshSource("Mesh Source", 0xFF9C27B0.toInt(), 45),
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
        val color: Int = 0xFF2196F3,
        // Proxy support
        var proxyPath: String = "",
        var useProxy: Boolean = false,
        var proxyResolution: String = "540p",
        var proxyGenerated: Boolean = false
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

    enum class ProceduralWaveType { Sine, Noise, Triangle, Square, Sawtooth }

    data class ProceduralConfig(
        var enabled: Boolean = false,
        var frequency: Float = 1.0f,      // wiggles per second
        var amplitude: Float = 10.0f,     // max displacement from base value
        var octaves: Int = 1,             // fractal noise layers
        var amplitudeMult: Float = 0.5f,  // per-octave amplitude falloff
        var waveType: ProceduralWaveType = ProceduralWaveType.Noise,
        var seed: Int = 0,                // for deterministic noise
        var phase: Float = 0.0f,          // time offset
    )

    data class KeyframeTrack(
        val keyframes: MutableList<Keyframe> = mutableListOf(),
        var procedural: ProceduralConfig = ProceduralConfig()
    ) {
        fun evaluate(time: Double): Float {
            val baseValue = if (keyframes.isEmpty()) 0f
            else if (keyframes.size == 1 || time <= keyframes.first().time) keyframes.first().value
            else if (time >= keyframes.last().time) keyframes.last().value
            else {
                val next = keyframes.firstOrNull { it.time > time } ?: return keyframes.last().value
                val prev = keyframes.lastOrNull { it.time <= time } ?: return keyframes.first().value

                val span = next.time - prev.time
                val t = if (span > 0) (time - prev.time) / span else 0.0

                when (prev.interpolation) {
                    InterpolationType.Step -> prev.value
                    InterpolationType.Linear -> prev.value + (next.value - prev.value) * t.toFloat()
                    InterpolationType.Bezier -> evaluateBezier(prev, next, t)
                    InterpolationType.Custom -> evaluateCustom(prev, next, t)
                }
            }

            // Apply procedural on top
            if (procedural.enabled) {
                baseValue + evaluateProcedural(time, procedural)
            } else {
                baseValue
            }
        }

        private fun evaluateProcedural(time: Double, config: ProceduralConfig): Float {
            val t = (time + config.phase.toDouble()) * config.frequency
            val seed = config.seed.toDouble()
            val amp = config.amplitude
            val octaves = config.octaves
            val ampMult = config.amplitudeMult

            var result = 0.0
            var freq = t
            var a = 1.0

            for (i in 0 until octaves) {
                val wave = when (config.waveType) {
                    ProceduralWaveType.Sine -> kotlin.math.sin(freq * 2.0 * kotlin.math.PI)
                    ProceduralWaveType.Noise -> simplexNoise1D(freq + seed * 1000.0 + i * 100.0)
                    ProceduralWaveType.Triangle -> 2.0 * abs((freq % 1.0) - 0.5) - 0.5
                    ProceduralWaveType.Square -> if (freq % 1.0 < 0.5) 1.0 else -1.0
                    ProceduralWaveType.Sawtooth -> 2.0 * (freq % 1.0) - 1.0
                }
                result += wave * a
                freq *= 2.0
                a *= ampMult.toDouble()
            }

            return (result * amp).toFloat()
        }

        private fun simplexNoise1D(x: Double): Double {
            // Simple 1D noise using hash-based gradient noise
            val i = kotlin.math.floor(x).toLong()
            val f = x - i.toDouble()
            val u = f * f * (3.0 - 2.0 * f) // smoothstep
            val a = hash11(i + config.seed.toLong())
            val b = hash11(i + 1 + config.seed.toLong())
            return a + (b - a) * u
        }

        private fun hash11(n: Long): Double {
            var h = n
            h = (h ^ (h ushr 16)) * 0x85ebca6bL
            h = (h ^ (h ushr 13)) * 0xc2b2ae35L
            h = h ^ (h ushr 16)
            return (h.toDouble() / Long.MAX_VALUE) * 2.0 - 1.0
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
            val cp = prev.customCurvePoints
            if (cp.size < 4) return prev.value + (next.value - prev.value) * t.toFloat()
            val p0x = 0.0; val p0y = 0.0
            val p1x = cp[0].toDouble(); val p1y = cp[1].toDouble()
            val p2x = cp[2].toDouble(); val p2y = cp[3].toDouble()
            val p3x = 1.0; val p3y = 1.0
            val u = 1.0 - t
            val uu = u * u
            val uuu = uu * u
            val tt = t * t
            val ttt = tt * t
            val py = uuu * p0y + 3 * uu * t * p1y + 3 * u * tt * p2y + ttt * p3y
            return (prev.value + (next.value - prev.value) * py).toFloat()
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
        data class AddKeyframe(val nodeId: String, val uniformName: String, val keyframe: Keyframe) : HistoryAction()
        data class RemoveKeyframe(val nodeId: String, val uniformName: String, val keyframe: Keyframe) : HistoryAction()
        data class UpdateKeyframe(val nodeId: String, val uniformName: String, val oldKeyframe: Keyframe, val newKeyframe: Keyframe) : HistoryAction()
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
            is HistoryAction.AddKeyframe -> {
                val node = nodes.value[action.nodeId] ?: return
                val track = node.animatedUniforms.getOrPut(action.uniformName) { KeyframeTrack() }
                track.keyframes.add(action.keyframe)
                track.keyframes.sortBy { it.time }
            }
            is HistoryAction.RemoveKeyframe -> {
                val node = nodes.value[action.nodeId] ?: return
                val track = node.animatedUniforms[action.uniformName] ?: return
                track.keyframes.removeAll { it.time == action.keyframe.time && it.value == action.keyframe.value }
            }
            is HistoryAction.UpdateKeyframe -> {
                val node = nodes.value[action.nodeId] ?: return
                val track = node.animatedUniforms[action.uniformName] ?: return
                val idx = track.keyframes.indexOfFirst { it.time == action.newKeyframe.time && it.value == action.newKeyframe.value }
                if (idx != -1) {
                    track.keyframes[idx] = action.newKeyframe
                }
            }
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
            is HistoryAction.AddKeyframe -> {
                val node = nodes.value[action.nodeId] ?: return
                val track = node.animatedUniforms[action.uniformName] ?: return
                track.keyframes.removeAll { it.time == action.keyframe.time && it.value == action.keyframe.value }
            }
            is HistoryAction.RemoveKeyframe -> {
                val node = nodes.value[action.nodeId] ?: return
                val track = node.animatedUniforms.getOrPut(action.uniformName) { KeyframeTrack() }
                track.keyframes.add(action.keyframe)
                track.keyframes.sortBy { it.time }
            }
            is HistoryAction.UpdateKeyframe -> {
                val node = nodes.value[action.nodeId] ?: return
                val track = node.animatedUniforms[action.uniformName] ?: return
                val idx = track.keyframes.indexOfFirst { it.time == action.newKeyframe.time && it.value == action.newKeyframe.value }
                if (idx != -1) {
                    track.keyframes[idx] = action.oldKeyframe
                }
            }
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
            // Motion blur
            NodeType.MotionBlur, NodeType.DirectionalBlur, NodeType.TransformBlur -> {
                node.inputs.add(Port("input", Port.PortType.Input))
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            // Velocity/Time remap
            NodeType.VelocityGraph, NodeType.TimeRemap, NodeType.OpticalFlow -> {
                node.inputs.add(Port("input", Port.PortType.Input))
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            // Masking/Rotoscoping
            NodeType.BezierMask, NodeType.Rotoscoping, NodeType.RotoBrush -> {
                node.inputs.add(Port("input", Port.PortType.Input))
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            NodeType.Tracker -> {
                node.outputs.add(Port("transform", Port.PortType.Output))
                node.outputs.add(Port("position", Port.PortType.Output))
                node.outputs.add(Port("rotation", Port.PortType.Output))
                node.outputs.add(Port("scale", Port.PortType.Output))
            }
            // Particle system
            NodeType.ParticleEmitter -> {
                node.outputs.add(Port("particles", Port.PortType.Output))
            }
            NodeType.ParticleForces -> {
                node.inputs.add(Port("particles", Port.PortType.Input))
                node.outputs.add(Port("particles", Port.PortType.Output))
            }
            NodeType.ParticleRenderer -> {
                node.inputs.add(Port("particles", Port.PortType.Input))
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            // Shape2D System
            NodeType.ShapeRectangle, NodeType.ShapeEllipse, NodeType.ShapePolygon, NodeType.ShapeStar, NodeType.ShapePath -> {
                node.outputs.add(Port("shape", Port.PortType.Output))
            }
            NodeType.ShapeRender -> {
                node.inputs.add(Port("shape", Port.PortType.Input))
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            NodeType.ShapeMerge -> {
                node.inputs.add(Port("shapeA", Port.PortType.Input))
                node.inputs.add(Port("shapeB", Port.PortType.Input))
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            NodeType.ShapeTransform -> {
                node.inputs.add(Port("shape", Port.PortType.Input))
                node.outputs.add(Port("shape", Port.PortType.Output))
            }
            NodeType.ShapeStroke, NodeType.ShapeFill, NodeType.ShapeRepeater, NodeType.ShapeBoolean -> {
                node.inputs.add(Port("shape", Port.PortType.Input))
                node.outputs.add(Port("shape", Port.PortType.Output))
            }
            // 2.5D System (Z-axis for 2D planes)
            NodeType.Transform3D -> {
                node.inputs.add(Port("input", Port.PortType.Input))
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            NodeType.Camera3D -> {
                node.outputs.add(Port("viewProj", Port.PortType.Output))
                node.outputs.add(Port("view", Port.PortType.Output))
                node.outputs.add(Port("proj", Port.PortType.Output))
            }
            NodeType.DepthOfField -> {
                node.inputs.add(Port("color", Port.PortType.Input))
                node.inputs.add(Port("depth", Port.PortType.Input))
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            // Keying/Compositing
            NodeType.ChromaKey -> {
                node.inputs.add(Port("foreground", Port.PortType.Input))
                node.inputs.add(Port("background", Port.PortType.Input))
                node.outputs.add(Port("output", Port.PortType.Output))
            }
            // 3D Models
            NodeType.MeshSource -> {
                node.outputs.add(Port("output", Port.PortType.Output))
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
        val existing = track.keyframes.firstOrNull { Math.abs(it.time - time) < 0.001 }
        if (existing != null) {
            val oldKf = existing.copy()
            existing.value = value
            recordAction(HistoryAction.UpdateKeyframe(nodeId, uniformName, oldKf, existing.copy()))
        } else {
            val kf = Keyframe(time, value)
            track.keyframes.add(kf)
            track.keyframes.sortBy { it.time }
            recordAction(HistoryAction.AddKeyframe(nodeId, uniformName, kf.copy()))
        }
        node.uniforms[uniformName] = value
        nativeEngine.updateUniform(nodeId, uniformName, value)
    }

    fun removeKeyframe(nodeId: String, uniformName: String, keyframe: Keyframe) {
        val node = nodes.value[nodeId] ?: return
        val track = node.animatedUniforms[uniformName] ?: return
        val removed = track.keyframes.firstOrNull { it.time == keyframe.time && it.value == keyframe.value } ?: return
        track.keyframes.remove(removed)
        recordAction(HistoryAction.RemoveKeyframe(nodeId, uniformName, removed.copy()))
    }

    fun updateKeyframe(nodeId: String, uniformName: String, oldKeyframe: Keyframe, newKeyframe: Keyframe) {
        val node = nodes.value[nodeId] ?: return
        val track = node.animatedUniforms[uniformName] ?: return
        val idx = track.keyframes.indexOfFirst { it.time == oldKeyframe.time && it.value == oldKeyframe.value }
        if (idx != -1) {
            track.keyframes[idx] = newKeyframe.copy()
            track.keyframes.sortBy { it.time }
            recordAction(HistoryAction.UpdateKeyframe(nodeId, uniformName, oldKeyframe.copy(), newKeyframe.copy()))
        }
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

    // Proxy workflow
    fun generateProxy(clipId: String, resolution: String = "540p") {
        val clip = clips.value.firstOrNull { it.id == clipId } ?: return
        // In a real implementation, this would call native engine to generate proxy
        clip.proxyPath = "proxy_${clip.id}.mp4"
        clip.proxyResolution = resolution
        clip.proxyGenerated = true
        // nativeEngine.generateProxy(clipId, resolution)
    }

    fun toggleProxy(clipId: String) {
        val clip = clips.value.firstOrNull { it.id == clipId } ?: return
        if (!clip.proxyGenerated) {
            generateProxy(clipId)
        }
        clip.useProxy = !clip.useProxy
    }

    fun getProxyResolutionOptions(): List<String> {
        return listOf("270p", "360p", "540p", "720p", "1080p")
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