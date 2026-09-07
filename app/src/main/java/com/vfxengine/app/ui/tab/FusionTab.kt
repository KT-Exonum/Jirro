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
import androidx.compose.ui.text.font.FontFamily
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
        
        // Keyframe editor overlay
        if (state.keyframeEditorTarget != null) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color(0xCC000000))
            ) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth(0.7f)
                        .fillMaxHeight(0.8f)
                        .align(Alignment.Center)
                        .background(Color(0xFF1E1E1E))
                        .padding(8.dp)
                ) {
                    Column(modifier = Modifier.fillMaxSize()) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(
                                text = "Keyframe Editor — ${state.keyframeEditorTarget!!.nodeId} > ${state.keyframeEditorTarget!!.uniformName}",
                                color = Color.White,
                                fontSize = 14.sp,
                                fontWeight = FontWeight.Bold
                            )
                            IconButton(onClick = { state.keyframeEditorTarget = null }) {
                                Icon(
                                    painter = painterResource(id = android.R.drawable.ic_menu_close_clear_cancel),
                                    contentDescription = "Close keyframe editor",
                                    tint = Color.White
                                )
                            }
                        }
                        androidx.compose.material3.Divider(color = Color.White.copy(alpha = 0.1f))
                        com.vfxengine.app.ui.keyframe.KeyframeEditor(state = state)
                    }
                }
            }
        }
    }
}

// Keyframe editor overlay is shown when state.keyframeEditorTarget is set

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
        
        // Keyframe editor overlay
        if (state.keyframeEditorTarget != null) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color(0xCC000000))
            ) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth(0.7f)
                        .fillMaxHeight(0.8f)
                        .align(Alignment.Center)
                        .background(Color(0xFF1E1E1E))
                        .padding(8.dp)
                ) {
                    Column(modifier = Modifier.fillMaxSize()) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(
                                text = "Keyframe Editor — ${state.keyframeEditorTarget!!.nodeId} > ${state.keyframeEditorTarget!!.uniformName}",
                                color = Color.White,
                                fontSize = 14.sp,
                                fontWeight = FontWeight.Bold
                            )
                            IconButton(onClick = { state.keyframeEditorTarget = null }) {
                                Icon(
                                    painter = painterResource(id = android.R.drawable.ic_menu_close_clear_cancel),
                                    contentDescription = "Close keyframe editor",
                                    tint = Color.White
                                )
                            }
                        }
                        androidx.compose.material3.Divider(color = Color.White.copy(alpha = 0.1f))
                        com.vfxengine.app.ui.keyframe.KeyframeEditor(state = state)
                    }
                }
            }
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

                // Keying
                ToolCategorySection(
                    title = "Keying",
                    color = 0xFFE91E63.toInt(),
                    items = listOf(
                        ToolItem("Chroma Key", EditorState.NodeType.ChromaKey, android.R.drawable.ic_menu_crop),
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

                // Motion Effects (Alight Motion + DaVinci Resolve)
                ToolCategorySection(
                    title = "Motion",
                    color = 0xFFE91E63.toInt(),
                    items = listOf(
                        // Transform Motion
                        ToolItem("Oscillate", EditorState.NodeType.Oscillate, android.R.drawable.ic_media_ff),
                        ToolItem("Shake", EditorState.NodeType.Shake, android.R.drawable.ic_media_ff),
                        ToolItem("Random Displace", EditorState.NodeType.RandomDisplacement, android.R.drawable.ic_menu_rotate),
                        ToolItem("Pulse", EditorState.NodeType.Pulse, android.R.drawable.ic_media_play),
                        ToolItem("Swing", EditorState.NodeType.Swing, android.R.drawable.ic_menu_rotate),
                        ToolItem("Bounce", EditorState.NodeType.Bounce, android.R.drawable.ic_media_ff),
                        ToolItem("Elastic", EditorState.NodeType.Elastic, android.R.drawable.ic_menu_rotate),
                        // Camera Motion
                        ToolItem("Camera Shake", EditorState.NodeType.CameraShake, android.R.drawable.ic_media_ff),
                        ToolItem("Camera Shake Pro", EditorState.NodeType.CameraShakePro, android.R.drawable.ic_media_play),
                        ToolItem("Dynamic Zoom", EditorState.NodeType.DynamicZoom, android.R.drawable.ic_menu_zoom),
                        ToolItem("Zoom Blur", EditorState.NodeType.ZoomBlur, android.R.drawable.ic_menu_zoom),
                        ToolItem("Radial Blur", EditorState.NodeType.RadialBlur, android.R.drawable.ic_menu_zoom),
                        ToolItem("Motion Blur", EditorState.NodeType.MotionBlur, android.R.drawable.ic_media_ff),
                        ToolItem("Directional Blur", EditorState.NodeType.DirectionalBlur, android.R.drawable.ic_media_ff),
                        // Distortion Motion
                        ToolItem("Ripple", EditorState.NodeType.Ripple, android.R.drawable.ic_menu_rotate),
                        ToolItem("Wave", EditorState.NodeType.Wave, android.R.drawable.ic_menu_rotate),
                        ToolItem("Twist", EditorState.NodeType.Twist, android.R.drawable.ic_menu_crop),
                        ToolItem("Bulge/Pinch", EditorState.NodeType.Bulge, android.R.drawable.ic_menu_crop),
                        ToolItem("Vortex", EditorState.NodeType.Vortex, android.R.drawable.ic_menu_rotate),
                        // Stylize Motion
                        ToolItem("Glitch", EditorState.NodeType.Glitch, android.R.drawable.ic_media_next),
                        ToolItem("VHS", EditorState.NodeType.VHS, android.R.drawable.ic_media_rew),
                        ToolItem("Scanlines", EditorState.NodeType.Scanlines, android.R.drawable.ic_menu_gallery),
                        ToolItem("CRT", EditorState.NodeType.CRT, android.R.drawable.ic_menu_gallery),
                        ToolItem("Chromatic Aberration", EditorState.NodeType.ChromaticAberration, android.R.drawable.ic_menu_gallery),
                        ToolItem("RGB Shift", EditorState.NodeType.RGBShift, android.R.drawable.ic_menu_gallery),
                        // Time Motion
                        ToolItem("Time Stretch", EditorState.NodeType.TimeStretch, android.R.drawable.ic_media_ff),
                        ToolItem("Frame Blend", EditorState.NodeType.FrameBlend, android.R.drawable.ic_media_play),
                        ToolItem("Stop Motion", EditorState.NodeType.StopMotion, android.R.drawable.ic_media_pause),
                        ToolItem("Posterize Time", EditorState.NodeType.PosterizeTime, android.R.drawable.ic_media_pause),
                        // Utility Motion
                        ToolItem("Wiggle", EditorState.NodeType.Wiggle, android.R.drawable.ic_media_ff),
                        ToolItem("Jitter", EditorState.NodeType.Jitter, android.R.drawable.ic_media_ff),
                        ToolItem("Drift", EditorState.NodeType.Drift, android.R.drawable.ic_menu_rotate),
                        ToolItem("Orbit", EditorState.NodeType.Orbit, android.R.drawable.ic_menu_rotate),
                        // Resolve FX
                        ToolItem("Film Damage", EditorState.NodeType.FilmDamage, android.R.drawable.ic_media_rew),
                        ToolItem("Film Grain", EditorState.NodeType.FilmGrain, android.R.drawable.ic_menu_gallery),
                        ToolItem("Vignette", EditorState.NodeType.Vignette, android.R.drawable.ic_menu_gallery),
                        ToolItem("Letterbox", EditorState.NodeType.Letterbox, android.R.drawable.ic_menu_crop),
                        // Advanced
                        ToolItem("Bezier Warp", EditorState.NodeType.BezierWarp, android.R.drawable.ic_menu_crop),
                        ToolItem("Mesh Warp", EditorState.NodeType.MeshWarpAdvanced, android.R.drawable.ic_menu_agenda),
                        ToolItem("Polar Coordinates", EditorState.NodeType.PolarCoordinates, android.R.drawable.ic_menu_rotate),
                        ToolItem("Displacement Map", EditorState.NodeType.DisplacementMap, android.R.drawable.ic_menu_gallery),
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

                // 3D Models
                ToolCategorySection(
                    title = "3D Models",
                    color = 0xFF9C27B0.toInt(),
                    items = listOf(
                        ToolItem("Mesh Source", EditorState.NodeType.MeshSource, android.R.drawable.ic_menu_gallery),
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

                // Audio/Reactive
                ToolCategorySection(
                    title = "Audio Reactive",
                    color = 0xFFE91E63.toInt(),
                    items = listOf(
                        ToolItem("Audio Reactive", EditorState.NodeType.AudioReactive, android.R.drawable.ic_media_play),
                        ToolItem("Audio Spectrum", EditorState.NodeType.AudioSpectrum, android.R.drawable.ic_menu_gallery),
                        ToolItem("Audio Waveform", EditorState.NodeType.AudioWaveform, android.R.drawable.ic_menu_gallery),
                        ToolItem("Beat Detect", EditorState.NodeType.BeatDetect, android.R.drawable.ic_media_pause),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Text/Typography
                ToolCategorySection(
                    title = "Text & Typography",
                    color = 0xFF795548.toInt(),
                    items = listOf(
                        ToolItem("Text Animator", EditorState.NodeType.TextAnimator, android.R.drawable.ic_menu_edit),
                        ToolItem("Text on Path", EditorState.NodeType.TextPath, android.R.drawable.ic_menu_crop),
                        ToolItem("Typewriter", EditorState.NodeType.Typewriter, android.R.drawable.ic_menu_edit),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Generators
                ToolCategorySection(
                    title = "Generators",
                    color = 0xFF00BCD4.toInt(),
                    items = listOf(
                        ToolItem("Gradient", EditorState.NodeType.Gradient, android.R.drawable.ic_menu_gallery),
                        ToolItem("Noise", EditorState.NodeType.Noise, android.R.drawable.ic_menu_gallery),
                        ToolItem("Checkerboard", EditorState.NodeType.Checkerboard, android.R.drawable.ic_menu_gallery),
                        ToolItem("Solid Color", EditorState.NodeType.SolidColor, android.R.drawable.ic_menu_gallery),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Distortion/Warping
                ToolCategorySection(
                    title = "Distortion",
                    color = 0xFF9C27B0.toInt(),
                    items = listOf(
                        ToolItem("Displace", EditorState.NodeType.Displace, android.R.drawable.ic_menu_crop),
                        ToolItem("Turbulent Displace", EditorState.NodeType.TurbulentDisplace, android.R.drawable.ic_menu_rotate),
                        ToolItem("Mesh Warp", EditorState.NodeType.MeshWarp, android.R.drawable.ic_menu_agenda),
                        ToolItem("Lens Distortion", EditorState.NodeType.LensDistortion, android.R.drawable.ic_menu_rotate),
                        ToolItem("Spherize", EditorState.NodeType.Spherize, android.R.drawable.ic_menu_crop),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Stylize
                ToolCategorySection(
                    title = "Stylize",
                    color = 0xFFFF9800.toInt(),
                    items = listOf(
                        ToolItem("Glow", EditorState.NodeType.Glow, android.R.drawable.ic_media_play),
                        ToolItem("Drop Shadow", EditorState.NodeType.DropShadow, android.R.drawable.ic_menu_crop),
                        ToolItem("Outline", EditorState.NodeType.Outline, android.R.drawable.ic_menu_edit),
                        ToolItem("Cartoon", EditorState.NodeType.Cartoon, android.R.drawable.ic_menu_gallery),
                        ToolItem("Halftone", EditorState.NodeType.Halftone, android.R.drawable.ic_menu_gallery),
                        ToolItem("VHS/Damage", EditorState.NodeType.VHS, android.R.drawable.ic_media_rew),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Time
                ToolCategorySection(
                    title = "Time",
                    color = 0xFF00BCD4.toInt(),
                    items = listOf(
                        ToolItem("Echo/Trails", EditorState.NodeType.Echo, android.R.drawable.ic_media_ff),
                        ToolItem("Frame Hold", EditorState.NodeType.FrameHold, android.R.drawable.ic_media_pause),
                        ToolItem("Time Offset", EditorState.NodeType.TimeOffset, android.R.drawable.ic_media_rew),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Color Grading
                ToolCategorySection(
                    title = "Color Grading",
                    color = 0xFFF44336.toInt(),
                    items = listOf(
                        ToolItem("Lift/Gamma/Gain", EditorState.NodeType.LiftGammaGain, android.R.drawable.ic_menu_gallery),
                        ToolItem("Color Wheels", EditorState.NodeType.ColorWheels, android.R.drawable.ic_menu_gallery),
                        ToolItem("RGB Curves", EditorState.NodeType.Curves, android.R.drawable.ic_menu_gallery),
                        ToolItem("Hue vs Sat", EditorState.NodeType.HueVsSat, android.R.drawable.ic_menu_gallery),
                        ToolItem("LUT", EditorState.NodeType.LUT, android.R.drawable.ic_menu_gallery),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Transitions
                ToolCategorySection(
                    title = "Transitions",
                    color = 0xFF3F51B5.toInt(),
                    items = listOf(
                        ToolItem("Cross Dissolve", EditorState.NodeType.CrossDissolve, android.R.drawable.ic_media_ff),
                        ToolItem("Dip to Color", EditorState.NodeType.DipToColor, android.R.drawable.ic_menu_gallery),
                        ToolItem("Slide", EditorState.NodeType.Slide, android.R.drawable.ic_menu_crop),
                        ToolItem("Push", EditorState.NodeType.Push, android.R.drawable.ic_media_ff),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // 3D/Environment
                ToolCategorySection(
                    title = "3D Environment",
                    color = 0xFF673AB7.toInt(),
                    items = listOf(
                        ToolItem("Environment Light", EditorState.NodeType.EnvironmentLight, android.R.drawable.ic_menu_mapmode),
                        ToolItem("Fog/Atmosphere", EditorState.NodeType.Fog, android.R.drawable.ic_menu_rotate),
                    ),
                    onItemClick = { type -> addNodeOfType(state, type) }
                )

                // Tracking/Stabilize
                ToolCategorySection(
                    title = "Tracking & Stabilize",
                    color = 0xFF9C27B0.toInt(),
                    items = listOf(
                        ToolItem("Stabilize", EditorState.NodeType.Stabilize, android.R.drawable.ic_media_play),
                        ToolItem("Corner Pin", EditorState.NodeType.CornerPin, android.R.drawable.ic_menu_crop),
                        ToolItem("Planar Tracker", EditorState.NodeType.PlanarTracker, android.R.drawable.ic_menu_mapmode),
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
                        ToolItem("Switch", EditorState.NodeType.Switch, android.R.drawable.ic_media_ff),
                        ToolItem("Expression", EditorState.NodeType.Expression, android.R.drawable.ic_menu_edit),
                        ToolItem("Value", EditorState.NodeType.Value, android.R.drawable.ic_menu_gallery),
                        ToolItem("Random", EditorState.NodeType.Random, android.R.drawable.ic_media_rew),
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
                        val portIndex = if (drag.isOutput) {
                            node.outputs.indexOfFirst { it.name == drag.fromPort }
                        } else {
                            node.inputs.indexOfFirst { it.name == drag.fromPort }
                        }
                        val fromX = if (drag.isOutput) {
                            node.x + 180f + state.nodePanX * state.nodeZoom
                        } else {
                            node.x + state.nodePanX * state.nodeZoom
                        }
                        val fromY = node.y + 20 + portIndex * 36f * state.nodeZoom + state.nodePanY * state.nodeZoom

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
                            0f,
                            isOutput
                        )
                    },
                    onDrag = { change, dragAmount ->
                        state.dragState = EditorState.DragState.Connecting(
                            node.id,
                            port.name,
                            change.position.x,
                            change.position.y,
                            isOutput
                        )
                    },
                    onDragEnd = { 
                        // Check if we're over a valid target port
                        val targetPort = findTargetPortAtPosition(state, change.position.x, change.position.y)
                        if (targetPort != null) {
                            validateAndCreateConnection(state, node.id, port.name, targetPort)
                        }
                        state.dragState = null
                    }
                )
            }
    )
}

// Find a port at the given screen position
fun findTargetPortAtPosition(state: EditorState, x: Float, y: Float): TargetPort? {
    val zoom = state.nodeZoom
    val panX = state.nodePanX
    val panY = state.nodePanY
    
    for (node in state.nodes.value.values) {
        val nodeLeft = node.x * zoom + panX
        val nodeTop = node.y * zoom + panY
        val nodeRight = nodeLeft + 200f * zoom
        val nodeBottom = nodeTop + (maxOf(node.inputs.size, node.outputs.size) * 40f + 50f) * zoom
        
        if (x >= nodeLeft && x <= nodeRight && y >= nodeTop && y <= nodeBottom) {
            // Check input ports (left side)
            for ((index, port) in node.inputs.withIndex()) {
                val portX = nodeLeft
                val portY = nodeTop + 20f * zoom + index * 36f * zoom
                val portSize = 16f * zoom
                if (x >= portX && x <= portX + portSize && y >= portY && y <= portY + portSize) {
                    return TargetPort(node.id, port.name, false)
                }
            }
            // Check output ports (right side)
            for ((index, port) in node.outputs.withIndex()) {
                val portX = nodeRight - 16f * zoom
                val portY = nodeTop + 20f * zoom + index * 36f * zoom
                val portSize = 16f * zoom
                if (x >= portX && x <= portX + portSize && y >= portY && y <= portY + portSize) {
                    return TargetPort(node.id, port.name, true)
                }
            }
        }
    }
    return null
}

data class TargetPort(val nodeId: String, val portName: String, val isOutput: Boolean)

// Validate and create connection
fun validateAndCreateConnection(state: EditorState, fromNodeId: String, fromPortName: String, target: TargetPort) {
    // Can only connect output to input
    if (!target.isOutput) {
        val fromNode = state.nodes.value[fromNodeId]
        val toNode = state.nodes.value[target.nodeId]
        
        if (fromNode != null && toNode != null) {
            // Check if already connected
            val alreadyConnected = state.connections.value.any { c ->
                c.fromNodeId == fromNodeId && c.fromPort == fromPortName && c.toNodeId == target.nodeId && c.toPort == target.portName
            }
            
            if (!alreadyConnected) {
                // Basic type validation - for now allow any output to any input
                // In a full implementation, check port.valueType compatibility
                state.connect(fromNodeId, fromPortName, target.nodeId, target.portName)
            }
        }
    }
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
                    // Color wheels for color correction nodes
                    LiftGammaGainOffsetWheels(state, selectedNode)
                    
                    // Split Color Wheels (Shadows/Midtones/Highlights)
                    SplitColorWheels(state, selectedNode)
                    
                    // RGB Curves Editor
                    CurvesEditor(state, selectedNode)
                    
                    // HSL Qualifiers
                    HSLQualifiersPanel(state, selectedNode)
                    
                    // Fallback sliders for non-color nodes or additional controls
                    val isColorNode = selectedNode.type == EditorState.NodeType.ColorCorrection || 
                                      selectedNode.type == EditorState.NodeType.Adjustment
                    
                    if (!isColorNode) {
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
                    
                    // LUT Browser for LUT nodes
                    if (selectedNode.type == EditorState.NodeType.LUT) {
                        LUTBrowserPanel(state, selectedNode)
                    }
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

/**
 * Professional color wheel component for lift/gamma/gain/offset grading
 */
@Composable
fun ColorWheel(
    label: String,
    color: androidx.compose.ui.graphics.Color,
    onColorChange: (androidx.compose.ui.graphics.Color) -> Unit,
    modifier: Modifier = Modifier,
    wheelSize: Dp = 150.dp
) {
    var currentColor by remember { mutableStateOf(color) }
    var hue by remember { mutableStateOf(0f) }
    var saturation by remember { mutableStateOf(0f) }
    var value by remember { mutableStateOf(1f) }
    
    // Convert color to HSV
    val hsv = remember { mutableStateOf(Color.RGBToHSV(currentColor)) }
    hue = hsv.value[0] * 360f
    saturation = hsv.value[1]
    value = hsv.value[2]
    
    val wheelRadius = wheelSize / 2
    
    Box(
        modifier = modifier
            .width(wheelSize)
            .height(wheelSize + 40.dp)
            .padding(8.dp)
    ) {
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
        ) {
            Text(text = label, color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Medium)
            
            // Color wheel canvas
            Box(
                modifier = Modifier
                    .size(wheelSize)
                    .pointerInput(Unit) {
                        detectDragGestures(
                            onDrag = { change, dragAmount ->
                                val center = Offset(wheelRadius.value.toFloat(), wheelRadius.value.toFloat())
                                val touchPos = Offset(change.position.x, change.position.y)
                                val vector = touchPos - center
                                val distance = sqrt(vector.x * vector.x + vector.y * vector.y)
                                
                                if (distance <= wheelRadius.value.toFloat()) {
                                    val angle = atan2(vector.y, vector.x)
                                    val newHue = (angle + PI) / (2 * PI) * 360f
                                    val newSaturation = min(distance / wheelRadius.value.toFloat(), 1f)
                                    
                                    hue = newHue
                                    saturation = newSaturation
                                    
                                    val newColor = Color.HSVToColor(hue, saturation, value)
                                    currentColor = newColor
                                    onColorChange(newColor)
                                }
                            }
                        )
                    }
            ) {
                // Color wheel gradient
                Canvas(modifier = Modifier.size(wheelSize)) {
                    val center = Offset(size.width / 2f, size.height / 2f)
                    val radius = size.width / 2f
                    
                    // Draw hue ring
                    for (i in 0..360 step 2) {
                        val angle = (i * PI / 180f) - PI / 2
                        val nextAngle = ((i + 2) * PI / 180f) - PI / 2
                        val color = Color.HSVToColor(i.toFloat(), 1f, 1f)
                        
                        val path = Path()
                        path.moveTo(center.x, center.y)
                        path.lineTo(
                            center.x + cos(angle) * radius,
                            center.y + sin(angle) * radius
                        )
                        path.lineTo(
                            center.x + cos(nextAngle) * radius,
                            center.y + sin(nextAngle) * radius
                        )
                        path.close()
                        
                        drawPath(
                            path = path,
                            color = color
                        )
                    }
                    
                    // Draw saturation overlay (white to transparent radial gradient)
                    for (i in 0..radius step 2) {
                        val alpha = 1f - (i / radius)
                        drawCircle(
                            color = Color.White.copy(alpha = alpha * 0.3f),
                            radius = i.toFloat(),
                            center = center
                        )
                    }
                    
                    // Value overlay (black to transparent radial gradient from center)
                    for (i in 0..radius step 2) {
                        val alpha = (1f - value) * (1f - i / radius)
                        if (alpha > 0) {
                            drawCircle(
                                color = Color.Black.copy(alpha = alpha * 0.5f),
                                radius = i.toFloat(),
                                center = center
                            )
                        }
                    }
                    
                    // Indicator dot
                    val indicatorAngle = (hue / 360f) * 2 * PI - PI / 2
                    val indicatorRadius = saturation * radius
                    val indicatorX = center.x + cos(indicatorAngle) * indicatorRadius
                    val indicatorY = center.y + sin(indicatorAngle) * indicatorRadius
                    
                    drawCircle(
                        color = if (value > 0.5) Color.Black else Color.White,
                        radius = 8.dp.toPx(),
                        center = Offset(indicatorX, indicatorY)
                    )
                    drawCircle(
                        color = if (value > 0.5) Color.White else Color.Black,
                        radius = 6.dp.toPx(),
                        center = Offset(indicatorX, indicatorY)
                    )
                }
            }
            
            // Value slider
            Column(modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp)) {
                Text(text = "Value", color = Color.White.copy(alpha = 0.7f), fontSize = 10.sp)
                Slider(
                    modifier = Modifier.fillMaxWidth(),
                    value = value,
                    onValueChange = { v ->
                        value = v
                        val newColor = Color.HSVToColor(hue, saturation, value)
                        currentColor = newColor
                        onColorChange(newColor)
                    },
                    colors = androidx.compose.material3.SliderDefaults.colors(
                        thumbColor = Color.Cyan,
                        activeTrackColor = Color.Cyan
                    )
                )
            }
        }
    }
}

/**
 * Lift/Gamma/Gain/Offset color wheels panel
 */
@Composable
fun LiftGammaGainOffsetWheels(
    state: EditorState,
    node: EditorState.Node
) {
    val isColorNode = node.type == EditorState.NodeType.ColorCorrection || 
                      node.type == EditorState.NodeType.Adjustment
    
    if (!isColorNode) return
    
    Card(
        modifier = Modifier.fillMaxWidth().padding(16.dp),
        backgroundColor = Color(0xFF1E1E1E)
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            Text(text = "Lift / Gamma / Gain / Offset", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
            
            // Wheels row
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                ColorWheel(
                    label = "Lift",
                    color = Color.HSVToColor(
                        (node.uniforms["liftHue"] ?: 0f),
                        (node.uniforms["liftSat"] ?: 0f),
                        (node.uniforms["liftVal"] ?: 1f)
                    ),
                    onColorChange = { color ->
                        val hsv = Color.RGBToHSV(color)
                        state.updateNodeUniform(node.id, "liftHue", hsv[0] * 360f)
                        state.updateNodeUniform(node.id, "liftSat", hsv[1])
                        state.updateNodeUniform(node.id, "liftVal", hsv[2])
                        // Also update lift as single float for compatibility
                        state.updateNodeUniform(node.id, "lift", hsv[2] - 1f)
                    },
                    modifier = Modifier.weight(1f)
                )
                
                ColorWheel(
                    label = "Gamma",
                    color = Color.HSVToColor(
                        (node.uniforms["gammaHue"] ?: 0f),
                        (node.uniforms["gammaSat"] ?: 0f),
                        (node.uniforms["gammaVal"] ?: 1f)
                    ),
                    onColorChange = { color ->
                        val hsv = Color.RGBToHSV(color)
                        state.updateNodeUniform(node.id, "gammaHue", hsv[0] * 360f)
                        state.updateNodeUniform(node.id, "gammaSat", hsv[1])
                        state.updateNodeUniform(node.id, "gammaVal", hsv[2])
                        state.updateNodeUniform(node.id, "gamma", hsv[2])
                    },
                    modifier = Modifier.weight(1f)
                )
                
                ColorWheel(
                    label = "Gain",
                    color = Color.HSVToColor(
                        (node.uniforms["gainHue"] ?: 0f),
                        (node.uniforms["gainSat"] ?: 0f),
                        (node.uniforms["gainVal"] ?: 1f)
                    ),
                    onColorChange = { color ->
                        val hsv = Color.RGBToHSV(color)
                        state.updateNodeUniform(node.id, "gainHue", hsv[0] * 360f)
                        state.updateNodeUniform(node.id, "gainSat", hsv[1])
                        state.updateNodeUniform(node.id, "gainVal", hsv[2])
                        state.updateNodeUniform(node.id, "gain", hsv[2])
                    },
                    modifier = Modifier.weight(1f)
                )
                
                ColorWheel(
                    label = "Offset",
                    color = Color.HSVToColor(
                        (node.uniforms["offsetHue"] ?: 0f),
                        (node.uniforms["offsetSat"] ?: 0f),
                        (node.uniforms["offsetVal"] ?: 1f)
                    ),
                    onColorChange = { color ->
                        val hsv = Color.RGBToHSV(color)
                        state.updateNodeUniform(node.id, "offsetHue", hsv[0] * 360f)
                        state.updateNodeUniform(node.id, "offsetSat", hsv[1])
                        state.updateNodeUniform(node.id, "offsetVal", hsv[2])
                        state.updateNodeUniform(node.id, "offset", hsv[2] - 1f)
                    },
                    modifier = Modifier.weight(1f)
                )
            }
            
            }
            }
        }
    }
}

// ============================================================================
// Color Grading: RGB Curves Editor
// ============================================================================
@Composable
fun CurvesEditor(
    state: EditorState,
    node: EditorState.Node
) {
    val isColorNode = node.type == EditorState.NodeType.ColorCorrection || 
                      node.type == EditorState.NodeType.Adjustment ||
                      node.type == EditorState.NodeType.Curves
    
    if (!isColorNode) return
    
    Card(
        modifier = Modifier.fillMaxWidth().padding(16.dp),
        backgroundColor = Color(0xFF1E1E1E)
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            // Channel selector
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(text = "RGB Curves", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                
                androidx.compose.material3.TextButton(
                    onClick = { state.updateNodeUniform(node.id, "curvesReset", 1f) },
                    modifier = Modifier.height(32.dp).padding(horizontal = 12.dp),
                    colors = androidx.compose.material3.ButtonDefaults.textButtonColors(contentColor = Color.Cyan)
                ) {
                    Text(text = "Reset", fontSize = 12.sp)
                }
            }
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))
            
            // Channel tabs
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(4.dp)
            ) {
                listOf("Master", "Red", "Green", "Blue").forEach { channel ->
                    val isMaster = channel == "Master"
                    androidx.compose.material3.TextButton(
                        onClick = { 
                            state.updateNodeUniform(node.id, "curvesChannel", 
                                when(channel) { "Master" -> 0f; "Red" -> 1f; "Green" -> 2f; "Blue" -> 3f; else -> 0f })
                        },
                        modifier = Modifier
                            .weight(1f)
                            .height(32.dp)
                            .padding(horizontal = 8.dp),
                        colors = androidx.compose.material3.ButtonDefaults.textButtonColors(
                            containerColor = if ((node.uniforms["curvesChannel"] ?: 0f).toInt() == 
                                when(channel) { "Master" -> 0; "Red" -> 1; "Green" -> 2; "Blue" -> 3; else -> 0 }) 
                                Color.Cyan.copy(alpha = 0.2f) else Color.Transparent
                        )
                    ) {
                        Text(
                            text = channel,
                            color = if ((node.uniforms["curvesChannel"] ?: 0f).toInt() == 
                                when(channel) { "Master" -> 0; "Red" -> 1; "Green" -> 2; "Blue" -> 3; else -> 0 }) 
                                Color.Cyan else Color.White.copy(alpha = 0.7f),
                            fontSize = 11.sp
                        )
                    }
                }
            }
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))
            
            // Curves graph
            CurvesGraph(state, node)
            
            // Preset curves
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(text = "Presets:", color = Color.White.copy(alpha = 0.7f), fontSize = 11.sp)
                listOf("Linear", "Contrast", "Log", "Cineon", "S-Curve").forEach { preset ->
                    androidx.compose.material3.TextButton(
                        onClick = { state.updateNodeUniform(node.id, "curvesPreset", preset) },
                        modifier = Modifier.height(28.dp).padding(horizontal = 8.dp),
                        colors = androidx.compose.material3.ButtonDefaults.textButtonColors(
                            contentColor = Color.White.copy(alpha = 0.8f)
                        )
                    ) {
                        Text(text = preset, fontSize = 10.sp)
                    }
                }
            }
        }
    }
}

// Curves graph visualization and editor
@Composable
fun CurvesGraph(state: EditorState, node: EditorState.Node) {
    val channel = (node.uniforms["curvesChannel"] ?: 0f).toInt()
    val channelName = when(channel) { 0 -> "master"; 1 -> "red"; 2 -> "green"; 3 -> "blue"; else -> "master" }
    
    // Get or create keyframe track for this curve
    val curveKey = "curves_${channelName}"
    val curveTrack = remember { node.animatedUniforms.getOrPut(curveKey) { KeyframeTrack() } }
    
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .aspectRatio(1f)
            .background(Color(0xFF0D0D0D))
    ) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            val width = size.width
            val height = size.height
            val padding = 20f
            val graphWidth = width - 2 * padding
            val graphHeight = height - 2 * padding
            
            // Grid
            val gridColor = Color.White.copy(alpha = 0.05f)
            for (i in 0..4) {
                val x = padding + i * graphWidth / 4f
                drawLine(color = gridColor, start = Offset(x, padding), end = Offset(x, padding + graphHeight), strokeWidth = 1f)
                val y = padding + i * graphHeight / 4f
                drawLine(color = gridColor, start = Offset(padding, y), end = Offset(padding + graphWidth, y), strokeWidth = 1f)
            }
            
            // Diagonal reference line
            drawLine(
                color = Color.White.copy(alpha = 0.1f),
                start = Offset(padding, padding + graphHeight),
                end = Offset(padding + graphWidth, padding),
                strokeWidth = 1f
            )
            
            // Curve path
            val path = Path()
            val points = curveTrack.keyframes.sortedBy { it.time }
            if (points.size >= 2) {
                // Interpolate curve for smooth drawing
                for (i in 0..100) {
                    val t = i / 100f
                    val value = curveTrack.evaluate(t)
                    val x = padding + t * graphWidth
                    val y = padding + graphHeight - value * graphHeight
                    if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
                }
            } else {
                // Default diagonal
                path.moveTo(padding, padding + graphHeight)
                path.lineTo(padding + graphWidth, padding)
            }
            
            drawPath(
                path = path,
                color = when(channel) { 0 -> Color.White; 1 -> Color.Red; 2 -> Color.Green; 3 -> Color.Blue; else -> Color.White },
                style = androidx.compose.ui.graphics.Stroke(width = 2f, cap = androidx.compose.ui.graphics.StrokeCap.Round)
            )
            
            // Control points
            points.forEach { kf ->
                val x = padding + (kf.time as Float) * graphWidth
                val y = padding + graphHeight - kf.value * graphHeight
                drawCircle(
                    color = Color.Cyan,
                    radius = 8.dp.toPx(),
                    center = Offset(x, y)
                )
                drawCircle(
                    color = Color.Black,
                    radius = 4.dp.toPx(),
                    center = Offset(x, y)
                )
            }
        }
        
        // Touch interaction for adding/moving points
        .pointerInput(Unit) {
            detectTapGestures(
                onTap = { offset ->
                    // Convert screen coordinates to curve coordinates
                    val x = (offset.x - 20f) / (size.width - 40f)
                    val y = 1f - (offset.y - 20f) / (size.height - 40f)
                    val clampedX = x.coerceIn(0f, 1f)
                    val clampedY = y.coerceIn(0f, 1f)
                    
                    // Add keyframe
                    val newKf = Keyframe(
                        time = clampedX.toDouble(),
                        value = clampedY,
                        interpolation = InterpolationType.Bezier
                    )
                    curveTrack.keyframes.add(newKf)
                    curveTrack.keyframes.sortBy { it.time }
                    
                    // Update native engine
                    state.updateNodeUniform(node.id, curveKey, clampedY)
                }
            )
        }
    }
}

// ============================================================================
// Color Grading: HSL Qualifiers (Hue vs Sat, Hue vs Hue, Hue vs Lum, Sat vs Sat, Lum vs Sat)
// ============================================================================
@Composable
fun HSLQualifiersPanel(
    state: EditorState,
    node: EditorState.Node
) {
    val isColorNode = node.type == EditorState.NodeType.ColorCorrection || 
                      node.type == EditorState.NodeType.Adjustment ||
                      node.type == EditorState.NodeType.HueVsSat ||
                      node.type == EditorState.NodeType.HueVsHue ||
                      node.type == EditorState.NodeType.HueVsLum ||
                      node.type == EditorState.NodeType.SatVsSat ||
                      node.type == EditorState.NodeType.LumVsSat
    
    if (!isColorNode) return
    
    Card(
        modifier = Modifier.fillMaxWidth().padding(16.dp),
        backgroundColor = Color(0xFF1E1E1E)
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            // Qualifier type selector
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(text = "HSL Qualifiers", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                
                Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(4.dp)) {
                    listOf("Hue vs Sat", "Hue vs Hue", "Hue vs Lum", "Sat vs Sat", "Lum vs Sat").forEach { qualifier ->
                        androidx.compose.material3.TextButton(
                            onClick = { 
                                state.updateNodeUniform(node.id, "hslQualifierType", qualifier)
                                state.updateNodeUniform(node.id, "hslQualifierEnable", 1f)
                            },
                            modifier = Modifier
                                .weight(1f)
                                .height(32.dp)
                                .padding(horizontal = 4.dp),
                            colors = androidx.compose.material3.ButtonDefaults.textButtonColors(
                                containerColor = if ((node.uniforms["hslQualifierType"] ?: "Hue vs Sat") == qualifier) 
                                    Color.Cyan.copy(alpha = 0.2f) else Color.Transparent
                            )
                        ) {
                            Text(text = qualifier, color = if ((node.uniforms["hslQualifierType"] ?: "Hue vs Sat") == qualifier) Color.Cyan else Color.White.copy(alpha = 0.7f), fontSize = 10.sp)
                        }
                    }
                }
            }
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))
            
            // Qualifier curve editor (similar to curves but for HSL space)
            QualifierGraph(state, node)
            
            // Range selectors
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                QualifierRangeSlider("Hue Range", "hslHueRange", 0f, 360f, state, node)
                QualifierRangeSlider("Sat Range", "hslSatRange", 0f, 1f, state, node)
                QualifierRangeSlider("Lum Range", "hslLumRange", 0f, 1f, state, node)
            }
            
            // Softness/falloff
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                MasterSlider("Hue Softness", "hslHueSoft", 0f, 1f, state, node)
                MasterSlider("Sat Softness", "hslSatSoft", 0f, 1f, state, node)
                MasterSlider("Lum Softness", "hslLumSoft", 0f, 1f, state, node)
            }
        }
    }
}

@Composable
fun QualifierGraph(state: EditorState, node: EditorState.Node) {
    // Similar to CurvesGraph but for HSL qualifier curves
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .aspectRatio(1f)
            .background(Color(0xFF0D0D0D))
    ) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            // Draw HSL qualifier visualization
            val width = size.width
            val height = size.height
            val padding = 20f
            
            // Hue wheel background
            val centerX = width / 2f
            val centerY = height / 2f
            val outerRadius = minOf(width, height) / 2f - padding
            
            // Draw hue ring
            for (i in 0..360 step 2) {
                val angle = (i - 90) * Math.PI / 180.0
                val nextAngle = (i + 2 - 90) * Math.PI / 180.0
                val color = Color.HSVToColor((i / 360f), 1f, 1f)
                drawArc(
                    color = color,
                    startAngle = (i - 90f).toFloat(),
                    sweepAngle = 2f,
                    useCenter = true,
                    topLeft = Offset(centerX - outerRadius, centerY - outerRadius),
                    size = Size(outerRadius * 2, outerRadius * 2),
                    style = androidx.compose.ui.graphics.Stroke(width = 4f)
                )
            }
        }
    }
}

@Composable
fun QualifierRangeSlider(
    label: String,
    uniformBase: String,
    min: Float,
    max: Float,
    state: EditorState,
    node: EditorState.Node
) {
    Column(modifier = Modifier.weight(1f)) {
        Text(text = label, color = Color.White.copy(alpha = 0.7f), fontSize = 10.sp)
        
        var rangeStart by remember { mutableStateOf(node.uniforms["${uniformBase}Start"] ?: min) }
        var rangeEnd by remember { mutableStateOf(node.uniforms["${uniformBase}End"] ?: max) }
        
        androidx.compose.material3.RangeSlider(
            modifier = Modifier.fillMaxWidth(),
            start = rangeStart,
            end = rangeEnd,
            onStartChange = { v ->
                rangeStart = v
                state.updateNodeUniform(node.id, "${uniformBase}Start", v)
            },
            onEndChange = { v ->
                rangeEnd = v
                state.updateNodeUniform(node.id, "${uniformBase}End", v)
            },
            rangeStart = min,
            rangeEnd = max,
            colors = androidx.compose.material3.SliderDefaults.colors(
                thumbColor = Color.Cyan,
                activeTrackColor = Color.Cyan,
                inactiveTrackColor = Color.White.copy(alpha = 0.1f)
            )
        )
    }
}

// ============================================================================
// Color Grading: Split Color Wheels (Shadows / Midtones / Highlights)
// ============================================================================
@Composable
fun SplitColorWheels(
    state: EditorState,
    node: EditorState.Node
) {
    val isColorNode = node.type == EditorState.NodeType.ColorCorrection || 
                      node.type == EditorState.NodeType.Adjustment ||
                      node.type == EditorState.NodeType.ColorWheels
    
    if (!isColorNode) return
    
    Card(
        modifier = Modifier.fillMaxWidth().padding(16.dp),
        backgroundColor = Color(0xFF1E1E1E)
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            Text(text = "Split Color Wheels (Shadows / Midtones / Highlights)", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))
            
            // Range boundaries
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
            ) {
                MasterSlider("Shadows Max", "shadowMax", 0f, 1f, state, node)
                MasterSlider("Midtones Center", "midCenter", 0f, 1f, state, node)
                MasterSlider("Highlights Min", "highlightMin", 0f, 1f, state, node)
            }
            
            // Falloff controls
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
            ) {
                MasterSlider("Shadow Falloff", "shadowFalloff", 0f, 1f, state, node)
                MasterSlider("Highlight Falloff", "highlightFalloff", 0f, 1f, state, node)
            }
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))
            
            // Three color wheels
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                SplitColorWheel(
                    label = "Shadows",
                    color = Color.HSVToColor(
                        (node.uniforms["shadowHue"] ?: 0f),
                        (node.uniforms["shadowSat"] ?: 0f),
                        (node.uniforms["shadowVal"] ?: 1f)
                    ),
                    onColorChange = { color ->
                        val hsv = Color.RGBToHSV(color)
                        state.updateNodeUniform(node.id, "shadowHue", hsv[0] * 360f)
                        state.updateNodeUniform(node.id, "shadowSat", hsv[1])
                        state.updateNodeUniform(node.id, "shadowVal", hsv[2])
                        state.updateNodeUniform(node.id, "shadows", hsv[2] - 1f)
                    },
                    modifier = Modifier.weight(1f)
                )
                
                SplitColorWheel(
                    label = "Midtones",
                    color = Color.HSVToColor(
                        (node.uniforms["midHue"] ?: 0f),
                        (node.uniforms["midSat"] ?: 0f),
                        (node.uniforms["midVal"] ?: 1f)
                    ),
                    onColorChange = { color ->
                        val hsv = Color.RGBToHSV(color)
                        state.updateNodeUniform(node.id, "midHue", hsv[0] * 360f)
                        state.updateNodeUniform(node.id, "midSat", hsv[1])
                        state.updateNodeUniform(node.id, "midVal", hsv[2])
                        state.updateNodeUniform(node.id, "midtones", hsv[2])
                    },
                    modifier = Modifier.weight(1f)
                )
                
                SplitColorWheel(
                    label = "Highlights",
                    color = Color.HSVToColor(
                        (node.uniforms["highlightHue"] ?: 0f),
                        (node.uniforms["highlightSat"] ?: 0f),
                        (node.uniforms["highlightVal"] ?: 1f)
                    ),
                    onColorChange = { color ->
                        val hsv = Color.RGBToHSV(color)
                        state.updateNodeUniform(node.id, "highlightHue", hsv[0] * 360f)
                        state.updateNodeUniform(node.id, "highlightSat", hsv[1])
                        state.updateNodeUniform(node.id, "highlightVal", hsv[2])
                        state.updateNodeUniform(node.id, "highlights", hsv[2] - 1f)
                    },
                    modifier = Modifier.weight(1f)
                )
            }
            
            // Master controls
            Row(
                modifier = Modifier.fillMaxWidth().padding(top = 16.dp),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
            ) {
                MasterSlider("Contrast", "contrast", 0.5f, 2f, state, node)
                MasterSlider("Pivot", "pivot", 0f, 1f, state, node)
                MasterSlider("Saturation", "saturation", 0f, 2f, state, node)
                MasterSlider("Temp", "temperature", -1f, 1f, state, node)
                MasterSlider("Tint", "tint", -1f, 1f, state, node)
            }
        }
    }
}

@Composable
fun SplitColorWheel(
    label: String,
    color: androidx.compose.ui.graphics.Color,
    onColorChange: (androidx.compose.ui.graphics.Color) -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(text = label, color = Color.White.copy(alpha = 0.7f), fontSize = 10.sp)
        androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))
        
        // Use the existing ColorWheel but with fixed size
        ColorWheel(
            label = "",
            color = color,
            onColorChange = onColorChange,
            modifier = Modifier.size(120.dp),
            wheelSize = 120.dp
        )
    }
}

@Composable
fun MasterSlider(
    label: String,
    uniformName: String,
    min: Float,
    max: Float,
    state: EditorState,
    node: EditorState.Node
) {
    Column(modifier = Modifier.weight(1f)) {
        Text(text = label, color = Color.White.copy(alpha = 0.7f), fontSize = 10.sp)
        var currentValue by remember { mutableStateOf(node.uniforms[uniformName] ?: 1f) }
        
        Slider(
            modifier = Modifier.fillMaxWidth(),
            value = (currentValue - min) / (max - min),
            onValueChange = { ratio ->
                val newValue = min + ratio * (max - min)
                currentValue = newValue
                state.updateNodeUniform(node.id, uniformName, newValue)
            },
            colors = androidx.compose.material3.SliderDefaults.colors(
                thumbColor = Color.Cyan,
                activeTrackColor = Color.Cyan,
                inactiveTrackColor = Color.White.copy(alpha = 0.1f)
            )
        )
        Text(text = "%.2f".format(currentValue), color = Color.Cyan, fontSize = 10.sp, fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace)
    }
}

// Color scopes panel - Waveform, Vectorscope, Parade
@Composable
fun ScopesPanel(
    state: EditorState,
    scopeType: ScopeType = ScopeType.Waveform,
    onScopeTypeChange: (ScopeType) -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(200.dp)
            .background(Color(0xFF0D0D0D))
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            // Scope selector
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(text = "Scopes", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)) {
                    ScopeType.values().forEach { type ->
                        androidx.compose.material3.TextButton(
                            onClick = { onScopeTypeChange(type) },
                            modifier = Modifier.height(32.dp).padding(horizontal = 8.dp),
                            colors = androidx.compose.material3.ButtonDefaults.textButtonColors(
                                containerColor = if (scopeType == type) Color.Cyan.copy(alpha = 0.2f) else Color.Transparent
                            )
                        ) {
                            Text(
                                text = type.label,
                                color = if (scopeType == type) Color.Cyan else Color.White.copy(alpha = 0.7f),
                                fontSize = 12.sp
                            )
                        }
                    }
                }
            }
            
            androidx.compose.material3.Divider(color = Color.White.copy(alpha = 0.1f))
            
            // Scope display
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black)
            ) {
                when (scopeType) {
                    ScopeType.Waveform -> WaveformScope()
                    ScopeType.Vectorscope -> VectorscopeScope()
                    ScopeType.Parade -> ParadeScope()
                }
            }
        }
    }
}

enum class ScopeType(val label: String) {
    Waveform("Waveform"),
    Vectorscope("Vectorscope"),
    Parade("Parade")
}

@Composable
fun WaveformScope() {
    Canvas(modifier = Modifier.fillMaxSize()) {
        val width = size.width
        val height = size.height
        val centerY = height / 2
        
        // Draw grid
        drawGrid(width, height)
        
        // Draw placeholder waveform (in real impl, get from engine)
        val path = Path()
        val dataSize = 256
        val step = width / dataSize
        for (i in 0..dataSize) {
            val x = i * step
            // Simulated waveform with some noise
            val noise = (Math.sin(i * 0.1) * 0.5 + Math.sin(i * 0.05) * 0.3 + (Math.random() - 0.5) * 0.2)
            val y = centerY - (noise * centerY * 0.8f)
            if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
        }
        drawPath(
            path = path,
            color = Color.Green,
            style = androidx.compose.ui.graphics.Stroke(width = 1.5f)
        )
        
        // Draw zero line
        drawLine(
            color = Color.White.copy(alpha = 0.3f),
            start = Offset(0f, centerY),
            end = Offset(width, centerY),
            strokeWidth = 1f
        )
    }
}

@Composable
fun VectorscopeScope() {
    Canvas(modifier = Modifier.fillMaxSize()) {
        val width = size.width
        val height = size.height
        val center = Offset(width / 2f, height / 2f)
        val radius = minOf(width, height) / 2f * 0.9f
        
        // Draw circular graticule
        drawCircle(
            color = Color.White.copy(alpha = 0.1f),
            radius = radius,
            center = center,
            style = androidx.compose.ui.graphics.Stroke(width = 1f)
        )
        drawCircle(
            color = Color.White.copy(alpha = 0.1f),
            radius = radius * 0.7f,
            center = center,
            style = androidx.compose.ui.graphics.Stroke(width = 1f)
        )
        
        // Draw I and Q axes
        drawLine(
            color = Color.White.copy(alpha = 0.2f),
            start = Offset(center.x - radius, center.y),
            end = Offset(center.x + radius, center.y),
            strokeWidth = 1f
        )
        drawLine(
            color = Color.White.copy(alpha = 0.2f),
            start = Offset(center.x, center.y - radius),
            end = Offset(center.x, center.y + radius),
            strokeWidth = 1f
        )
        
        // Draw color targets (simplified - real impl would plot U/V from frame)
        val targets = listOf(
            Triple(0.7f, 0.7f, "R"),   // Red
            Triple(-0.3f, 0.7f, "G"),  // Green
            Triple(-0.3f, -0.7f, "B"), // Blue
            Triple(0.7f, -0.7f, "Y"),  // Yellow
            Triple(-0.7f, -0.7f, "C"), // Cyan
            Triple(0.7f, 0.3f, "M"),   // Magenta
        )
        
        for ((iq, label) in targets) {
            val x = center.x + iq.first * radius
            val y = center.y - iq.second * radius
            drawCircle(
                color = Color.White.copy(alpha = 0.5f),
                radius = 8.dp.toPx(),
                center = Offset(x, y)
            )
            drawText(
                text = label,
                color = Color.White,
                fontSize = 10.sp,
                topLeft = Offset(x + 10.dp.toPx(), y - 10.dp.toPx())
            )
        }
    }
}

@Composable
fun ParadeScope() {
    Canvas(modifier = Modifier.fillMaxSize()) {
        val width = size.width
        val height = size.height
        val thirdHeight = height / 3f
        
        // Draw three waveform panels (R, G, B)
        for (ch in 0..2) {
            val top = ch * thirdHeight
            val centerY = top + thirdHeight / 2f
            
            // Channel label
            drawText(
                text = listOf("R", "G", "B")[ch],
                color = listOf(Color.Red, Color.Green, Color.Blue)[ch],
                fontSize = 12.sp,
                topLeft = Offset(4f, top + 4f)
            )
            
            // Draw waveform for this channel (simulated)
            val path = Path()
            val dataSize = 256
            val step = width / dataSize
            for (i in 0..dataSize) {
                val x = i * step
                val noise = (Math.sin(i * 0.1) * 0.5 + Math.sin(i * 0.05) * 0.3 + (Math.random() - 0.5) * 0.2)
                val y = centerY - (noise * thirdHeight * 0.4f)
                if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
            }
            drawPath(
                path = path,
                color = listOf(Color.Red, Color.Green, Color.Blue)[ch],
                style = androidx.compose.ui.graphics.Stroke(width = 1.5f)
            )
        }
    }
}

@Composable
fun drawGrid(width: Float, height: Float) {
    val centerY = height / 2
    val gridColor = Color.White.copy(alpha = 0.05f)
    
    // Horizontal lines
    for (i in 0..4) {
        val y = i * height / 4f
        drawLine(
            color = gridColor,
            start = Offset(0f, y),
            end = Offset(width, y),
            strokeWidth = 1f
        )
    }
    
    // Vertical lines
    for (i in 0..10) {
        val x = i * width / 10f
        drawLine(
            color = gridColor,
            start = Offset(x, 0f),
            end = Offset(x, height),
            strokeWidth = 1f
        )
    }
}

// LUT Browser Panel for LUT nodes
@Composable
fun LUTBrowserPanel(state: EditorState, node: EditorState.Node) {
    Card(
        modifier = Modifier.fillMaxWidth().padding(16.dp),
        backgroundColor = Color(0xFF1E1E1E)
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            Text(text = "3D LUT Browser", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))
            
            // LUT selection
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(text = "LUT: ${node.uniforms["lutName"]?.toString() ?: "None"}", color = Color.White.copy(alpha = 0.8f), fontSize = 13.sp)
                
                androidx.compose.material3.OutlinedButton(
                    onClick = { /* Open LUT picker */ },
                    modifier = Modifier.height(36.dp).padding(horizontal = 16.dp),
                    colors = androidx.compose.material3.ButtonDefaults.outlinedButtonColors(
                        contentColor = Color.Cyan,
                        borderColor = Color.Cyan.copy(alpha = 0.5f)
                    )
                ) {
                    Text(text = "Browse", fontSize = 12.sp, fontWeight = FontWeight.Medium)
                }
            }
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))
            
            // LUT parameters
            LUTSlider("Intensity", "lutIntensity", 0f, 1f, state, node)
            LUTSlider("Contrast", "lutContrast", -1f, 1f, state, node)
            LUTSlider("Saturation", "lutSaturation", 0f, 2f, state, node)
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))
            
            // Interpolation method
            Text(text = "Interpolation", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
            ) {
                val interpMethods = listOf("Nearest", "Trilinear", "Tetrahedral")
                interpMethods.forEach { method ->
                    androidx.compose.material3.TextButton(
                        onClick = { state.updateNodeUniform(node.id, "lutInterpolation", interpMethods.indexOf(method).toFloat()) },
                        modifier = Modifier.weight(1f).height(36.dp),
                        colors = androidx.compose.material3.ButtonDefaults.textButtonColors(
                            containerColor = (node.uniforms["lutInterpolation"] ?: 0f).toInt() == interpMethods.indexOf(method) ? Color.Cyan.copy(alpha = 0.2f) : Color.Transparent
                        )
                    ) {
                        Text(text = method, color = if ((node.uniforms["lutInterpolation"] ?: 0f).toInt() == interpMethods.indexOf(method)) Color.Cyan else Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
                    }
                }
            }
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))
            
            // Color space
            Text(text = "Input Color Space", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(8.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
            ) {
                val colorSpaces = listOf("sRGB", "Rec.709", "LogC", "S-Log")
                colorSpaces.forEach { cs ->
                    androidx.compose.material3.TextButton(
                        onClick = { state.updateNodeUniform(node.id, "lutColorSpace", colorSpaces.indexOf(cs).toFloat()) },
                        modifier = Modifier.weight(1f).height(36.dp),
                        colors = androidx.compose.material3.ButtonDefaults.textButtonColors(
                            containerColor = (node.uniforms["lutColorSpace"] ?: 0f).toInt() == colorSpaces.indexOf(cs) ? Color.Cyan.copy(alpha = 0.2f) : Color.Transparent
                        )
                    ) {
                        Text(text = cs, color = if ((node.uniforms["lutColorSpace"] ?: 0f).toInt() == colorSpaces.indexOf(cs)) Color.Cyan else Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
                    }
                }
            }
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))
            
            // Preview toggle
            androidx.compose.material3.IconButton(
                onClick = { 
                    val current = node.uniforms["lutPreview"] ?: 0f
                    state.updateNodeUniform(node.id, "lutPreview", if (current > 0.5f) 0f else 1f)
                },
                modifier = Modifier.size(40.dp),
                colors = androidx.compose.material3.IconButtonDefaults.iconButtonColors(
                    containerColor = (node.uniforms["lutPreview"] ?: 0f) > 0.5f ? Color.Cyan.copy(alpha = 0.2f) : Color.Transparent
                )
            ) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_zoom),
                    contentDescription = "Toggle LUT Preview",
                    tint = if ((node.uniforms["lutPreview"] ?: 0f) > 0.5f) Color.Cyan else Color.White.copy(alpha = 0.7f)
                )
            }
        }
    }
}

@Composable
fun LUTSlider(
    label: String,
    uniformName: String,
    min: Float,
    max: Float,
    state: EditorState,
    node: EditorState.Node
) {
    Column(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
        ) {
            Text(text = label, color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
            Text(text = "%.2f".format(node.uniforms[uniformName] ?: (if (uniformName == "lutIntensity") 1f else if (uniformName == "lutContrast") 0f else 1f)), color = Color.Cyan, fontSize = 12.sp, fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace)
        }
        
        var currentValue by remember { mutableStateOf(node.uniforms[uniformName] ?: (if (uniformName == "lutIntensity") 1f else if (uniformName == "lutContrast") 0f else 1f)) }
        
        androidx.compose.material3.Slider(
            modifier = Modifier.fillMaxWidth(),
            value = (currentValue - min) / (max - min),
            onValueChange = { ratio ->
                val newValue = min + ratio * (max - min)
                currentValue = newValue
                state.updateNodeUniform(node.id, uniformName, newValue)
            },
            colors = androidx.compose.material3.SliderDefaults.colors(
                thumbColor = Color.Cyan,
                activeTrackColor = Color.Cyan,
                inactiveTrackColor = Color.White.copy(alpha = 0.1f)
            )
        )
    }
}