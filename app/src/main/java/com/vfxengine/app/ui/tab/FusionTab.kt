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
                    // Color wheels for color correction nodes
                    LiftGammaGainOffsetWheels(state, selectedNode)
                    
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