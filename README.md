# VFX Engine — Android Node-Based Video/VFX Editor

A desktop-grade, node-based VFX + video editing engine for Android:
Kotlin/Compose UI ⇄ (async JNI) ⇄ C++23 engine ⇄ Vulkan (primary) / GLES 3.2 (fallback).

This repo is built **phase by phase**, per the project spec. Building all
subsystems at once produces something nobody can debug. Each phase below is a
self-contained, testable milestone, and the abstractions are designed so a
team can work on different phases in parallel without stepping on each other.

## What's implemented now vs. scaffolded

| Phase | Status | Where |
|---|---|---|
| 1. Native rendering sandbox (Vulkan triangle) | **Fully implemented** | `engine/vulkan/VulkanDevice.*` |
| 2. GPU video pipeline (AMediaCodec → Vulkan image) | **Fully implemented** — real `AMediaExtractor`/`AMediaCodec`/`AImageReader` decode, zero-copy `AHardwareBuffer` → `VkImage` import with `VK_KHR_sampler_ycbcr_conversion`, decoder pool + frame cache, dedicated media thread | `engine/media/*`, `engine/vulkan/VulkanDevice.cpp` (`ImportHardwareBuffer`, `CreateTexture`) |
| 3. Render graph & compositing | **Largely implemented.** Dependency resolution/topo-sort, per-`NodeKind` shader registry (~80 effects, see `GetShaderRegistry()`), keyframe/expression/audio-reactive uniform injection, and pass execution (Shader/Blend/Composite/Output/Motion/Shape2D/Particle/etc.) all run for real. Known gaps below. | `engine/graph/*` |
| 4. Timeline & keyframes | **Fully implemented** (pure logic, no GPU dependency) | `engine/timeline/*` |
| 5. Compose frontend | **Partial.** Edit/Fusion/Media/Deliver tabs, node editor, keyframe editor, and inspector all exist and are wired to JNI, but the on-canvas preview has no direct-manipulation gestures yet (no drag/pinch/rotate handles on layers — see Known limitations) | `app/src/main/java/.../ui/*` |
| 6. Optimization & export | **Partial.** Export pipeline runs the real render graph and hardware encoder path per-frame; profiler/pooling hooks are live. Some export-path gaps remain (see below). | throughout |

### Phase 3 in detail

`RenderGraph::ExecutePass` resolves a shader per `NodeKind` (or a custom
SPIR-V fragment for `Shader` nodes) via `GetShaderRegistry()`/`ResolveShaderEntry()`,
evaluates that node's `KeyframeTrack`s, runs any `ExpressionEngine` scripts,
merges in audio-reactive uniforms when relevant, and draws through
`GraphicsDevice::DrawFullscreenPass` (or the particle compute/draw path for
particle nodes). This covers color/blur/mask/composite, the Motion Effects
family (oscillate/shake/wiggle/glitch/CRT/ripple/twist/etc.), Shape2D,
Transform3D/Camera3D, ChromaKey, text, and audio visualization nodes.

Known Phase 3 gaps:
- `Shape2D` boolean operations (union/subtract/intersect/xor) are **not
  implemented** — `Shape2D.cpp` currently returns the first operand
  unchanged. Needed for parity with any shape-boolean-combine workflow.
- `RenderGraph::ExecutePass` hardcodes the pass output resolution
  (1920×1080) and frame rate (30fps) instead of reading the active
  project/export settings — see `PackSimParams` and the `ctx.frameRate`
  TODO. Affects particle emission rates and any resolution-dependent effect
  when exporting at a non-default size/frame rate.
- `RenderParticles` (CPU-side particle draw path, as opposed to the GPU
  compute path) is an unimplemented fallback.

### Phase 2 in detail

The GPU video pipeline follows Section 4's pipeline exactly, with no CPU-side
YUV buffer ever produced:

```text
MP4 file
   ↓ AMediaExtractor (demux)
   ↓ AMediaCodec (hardware decode, output surface = AImageReader's window)
   ↓ AImageReader (AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE)
   ↓ AImage_getHardwareBuffer (zero copy)
   ↓ vkGetAndroidHardwareBufferPropertiesANDROID + VkImportAndroidHardwareBufferInfoANDROID
   ↓ VkImage + VkSamplerYcbcrConversion (YUV→RGB happens here, during sampling)
   ↓ MediaEngine::FrameCache (Section 9 windowed cache around the playhead)
   ↓ RenderGraph::ExecutePass (VideoSource) — Engine thread, non-blocking
```

Threading matches Section 14: `MediaEngine` owns a dedicated media thread
(`DecoderPool` + `FrameCache`, `engine/media/MediaEngine.cpp`) that the engine
thread never blocks on — `MediaEngine::TryGetFrame` is non-blocking and
`RenderGraph::ExecutePass` simply holds the last good frame for a `VideoSource`
node on a cache miss (see `RenderGraph.cpp`'s `lastVideoFrameByNode_`).

Known Phase 2 limitations, called out explicitly rather than glossed over:
- `Clip::ToSourceTime` mapping from timeline time to source time is wired
  through `Engine::RefreshActiveClips`, but `RenderGraph::ExecutePass` still
  receives a single `timelineSeconds` parameter for the whole pass tree —
  a Shader/Blend pass needing its own keyframe time *and* a VideoSource
  ancestor needing source time simultaneously isn't handled per-node yet.
- `DecoderPool`'s eviction policy is plain LRU; there's room for a smarter
  policy (e.g. weighting by proximity to active transitions).
- `FrameCache::EvictOutsideWindow` approximates the per-clip frame duration
  (24fps) rather than reading each clip's actual frame rate; harmless for
  correctness (it only affects memory-window sizing) but should be replaced
  once `VideoDecoder` exposes the source's frame rate from `AMediaFormat`.
- GLES fallback (`OpenGLDevice`) does not yet have a hardware-buffer import
  path (`EGL_ANDROID_get_native_client_buffer` + `GL_TEXTURE_EXTERNAL_OES`);
  the video pipeline currently requires the Vulkan backend, so devices
  without solid Vulkan 1.1 + AHardwareBuffer support can't play video at all.

### Phase 6 (export) in detail

`ExportPipeline` drives the real `RenderGraph::Compile`/`Execute` per output
frame against the hardware encoder, not a mock. Known gaps:
- **Expressions do not run during export.** `ExportPipeline`'s render call
  passes `expressionEngine = nullptr`, so any parameter driven by an
  `ExpressionEngine` script renders correctly in the live preview but
  silently falls back to its static value in the exported file. Keyframed
  (non-expression) parameters are unaffected.
- `ExportPipeline::GetMixedAudio`'s manual-mix fallback (used when
  `AudioEngine::GetMixedAudio` returns nothing) is an empty stub — it
  returns a chunk with no samples rather than actually mixing timeline
  clips.
- `MediaEngine`'s single-frame-to-file save path (used for thumbnail/still
  export) is a stub — it returns a placeholder path without writing the
  frame texture to disk.

## Known UI-layer gaps

- **No direct-manipulation canvas gestures.** `EngineSurfaceView.kt` only
  hands the `Surface` to the native swapchain; there's no touch handling on
  it, and `GestureShortcuts.kt` only wires pinch/pan/tap for the node editor
  and timeline. There's currently no way to drag/scale/rotate a layer by
  touching it in the preview.
- **Media library scanning is stubbed.** `EditorState.kt` doesn't scan
  device storage yet; `MediaTab.kt` renders placeholder thumbnails
  (`painterResource(id = 0)`) rather than real decoded ones.
- A few controls are inert placeholders pending wiring: the Deliver tab's
  export button, the Settings dialog's "new project"/"file picker" entries,
  and one persisted-default checkbox in the Edit tab.
- `NativeEngine.kt` currently has duplicate method declarations
  (`startAudioOutput`, `stopAudioOutput`, `getProfileStats`, `reloadShaders`
  are each declared twice) — needs deduping before this will compile.

## Build

Requires Android Studio (Koala+), NDK 26+, CMake 3.22+, a device/emulator with
Vulkan 1.1 support for Phase 1 validation, and a device with hardware AVC/HEVC
decode support for Phase 2 validation (AHardwareBuffer + VK_KHR_sampler_ycbcr_conversion
require API 28+, matching `minSdk`).

```
./gradlew :app:assembleDebug
```

Shaders are pre-compiled to SPIR-V offline (see `shaders/README.md`) and
bundled as raw assets — no runtime `glslangValidator` in production builds,
per the spec's "prefer offline compilation" requirement. A runtime GLSL path
exists behind `ENGINE_DEV_SHADER_HOTLOAD` for iteration.

## Directory map

```
app/src/main/java/com/vfxengine/app/
  MainActivity.kt          - Compose host, owns lifecycle of NativeEngine
  NativeEngine.kt          - JNI bridge (Kotlin side): fire-and-forget commands
  ui/EngineSurfaceView.kt  - SurfaceView whose Surface is handed to native Vulkan swapchain
                             (no touch handling yet — see Known UI-layer gaps)
  ui/tab/                  - Edit / Fusion / Media / Deliver screens (Resolve-page-style layout)
  ui/keyframe/KeyframeEditor.kt, ui/nodeeditor/NodeEditor.kt, ui/inspector/InspectorPanel.kt
                           - node graph + keyframe + property UI, wired to JNI

app/src/main/cpp/
  CMakeLists.txt
  jni/jni_bridge.cpp       - JNI entry points, translate into Engine::QueueCommand
  Engine.h / Engine.cpp    - top-level owner: command queue, engine thread, subsystems,
                             owns MediaEngine (Phase 2) and refreshes its active-clip
                             set from Timeline every tick
  engine/core/
    CommandQueue.h         - lock-free MPSC queue, UI thread never blocks on engine
    GraphicsDevice.h        - backend-agnostic interface (Vulkan primary, GLES fallback)
    Types.h                 - shared handle/resource types
  engine/vulkan/
    VulkanDevice.h/.cpp     - Phase 1: instance/device/swapchain/pipeline/triangle;
                              Phase 2: CreateTexture, ImportHardwareBuffer (AHardwareBuffer
                              -> VkImage + VkSamplerYcbcrConversion), ycbcr conversion/sampler cache
  engine/opengl/
    OpenGLDevice.h          - GraphicsDevice impl stub (compatibility backend; no
                              hardware-buffer import path yet, see Phase 2 limitations above)
  engine/graph/
    Node.h                  - Node/Connection/Resource abstractions, ~90 NodeKinds
                              (sources, effects, motion/distortion/stylize/time effects,
                              Shape2D, 2.5D, particles, audio-reactive, text)
    RenderGraph.h/.cpp      - dependency resolution, topological execution order,
                              per-NodeKind shader registry, keyframe/expression/audio
                              uniform injection, resource-lifetime + pooling, particle
                              compute dispatch (Phase 3, see Known Phase 3 gaps)
  engine/timeline/
    Keyframe.h              - KeyframeTrack, interpolation (linear/step/bezier/custom)
    Timeline.h               - TimelineTime, deterministic Render(time) contract
  engine/media/
    MediaEngine.h/.cpp       - Phase 2: VideoDecoder (AMediaExtractor/AMediaCodec/
                               AImageReader), DecoderPool (bounded, LRU-evicted),
                               FrameCache (windowed around the playhead, mutex-guarded
                               for cross-thread reads), and the MediaEngine facade that
                               owns the dedicated media thread
  engine/expression/
    ExpressionEngine.h/.cpp - QuickJS-backed per-uniform scripting (AE/Alight-Motion-style
                              expressions); not currently evaluated on the export path
                              (see Phase 6 gaps)
  engine/shape2d/
    Shape2D.h/.cpp           - vector shape data + rendering; boolean ops unimplemented
                              (see Known Phase 3 gaps)
  engine/export/
    ExportPipeline.h/.cpp    - per-frame render-graph execution against the hardware
                              encoder; audio-mix fallback stubbed (see Phase 6 gaps)
    ProjectSerializer.h/.cpp - JSON project save/load, autosave, recent-projects list
```

## Full file tree

```
Jirro-857911ea94f53763afb1176c4e36e8bc2392b13c/
|-- app
|   |-- src
|   |   `-- main
|   |       |-- cpp
|   |       |   |-- engine
|   |       |   |   |-- 3d
|   |       |   |   |   |-- Light.cpp
|   |       |   |   |   |-- Light.h
|   |       |   |   |   |-- Material.cpp
|   |       |   |   |   |-- Material.h
|   |       |   |   |   |-- Shadow.cpp
|   |       |   |   |   |-- Shadow.h
|   |       |   |   |   |-- glTFLoader.cpp
|   |       |   |   |   `-- glTFLoader.h
|   |       |   |   |-- asset
|   |       |   |   |   |-- AssetManager.cpp
|   |       |   |   |   `-- AssetManager.h
|   |       |   |   |-- audio
|   |       |   |   |   |-- AudioEngine.cpp
|   |       |   |   |   |-- AudioEngine.h
|   |       |   |   |   |-- AudioOutput.cpp
|   |       |   |   |   `-- AudioOutput.h
|   |       |   |   |-- color
|   |       |   |   |   `-- ColorManagement.h
|   |       |   |   |-- core
|   |       |   |   |   |-- CommandQueue.h
|   |       |   |   |   |-- GraphicsDevice.h
|   |       |   |   |   |-- Profiler.cpp
|   |       |   |   |   |-- Profiler.h
|   |       |   |   |   |-- RuntimeShaderCompiler.cpp
|   |       |   |   |   |-- RuntimeShaderCompiler.h
|   |       |   |   |   `-- Types.h
|   |       |   |   |-- export
|   |       |   |   |   |-- ExportPipeline.cpp
|   |       |   |   |   |-- ExportPipeline.h
|   |       |   |   |   |-- ProjectSerializer.cpp
|   |       |   |   |   `-- ProjectSerializer.h
|   |       |   |   |-- expression
|   |       |   |   |   |-- ExpressionEngine.cpp
|   |       |   |   |   `-- ExpressionEngine.h
|   |       |   |   |-- graph
|   |       |   |   |   |-- Node.h
|   |       |   |   |   |-- ParticleSystem.h
|   |       |   |   |   |-- RenderGraph.cpp
|   |       |   |   |   `-- RenderGraph.h
|   |       |   |   |-- media
|   |       |   |   |   |-- MediaEngine.cpp
|   |       |   |   |   `-- MediaEngine.h
|   |       |   |   |-- opengl
|   |       |   |   |   `-- OpenGLDevice.h
|   |       |   |   |-- shape2d
|   |       |   |   |   |-- Shape2D.cpp
|   |       |   |   |   `-- Shape2D.h
|   |       |   |   |-- text
|   |       |   |   |   |-- TextRenderer.cpp
|   |       |   |   |   `-- TextRenderer.h
|   |       |   |   |-- timeline
|   |       |   |   |   |-- Keyframe.h
|   |       |   |   |   |-- Timeline.cpp
|   |       |   |   |   `-- Timeline.h
|   |       |   |   `-- vulkan
|   |       |   |       |-- VulkanDevice.cpp
|   |       |   |       `-- VulkanDevice.h
|   |       |   |-- jni
|   |       |   |   `-- jni_bridge.cpp
|   |       |   |-- shaders
|   |       |   |   |-- README.md
|   |       |   |   |-- adjustment.frag
|   |       |   |   |-- adjustment.spv
|   |       |   |   |-- adjustment.vert
|   |       |   |   |-- audio_reactive.frag
|   |       |   |   |-- audio_reactive.spv
|   |       |   |   |-- audio_spectrum.frag
|   |       |   |   |-- audio_spectrum.spv
|   |       |   |   |-- audio_waveform.frag
|   |       |   |   |-- audio_waveform.spv
|   |       |   |   |-- bezier_mask.frag
|   |       |   |   |-- bezier_mask.spv
|   |       |   |   |-- bezier_mask.vert
|   |       |   |   |-- bezier_warp.frag
|   |       |   |   |-- bezier_warp.spv
|   |       |   |   |-- blend_add.frag
|   |       |   |   |-- blend_add.spv
|   |       |   |   |-- blend_multiply.frag
|   |       |   |   |-- blend_multiply.spv
|   |       |   |   |-- blend_normal.frag
|   |       |   |   |-- blend_normal.spv
|   |       |   |   |-- blend_overlay.frag
|   |       |   |   |-- blend_overlay.spv
|   |       |   |   |-- blend_screen.frag
|   |       |   |   |-- blend_screen.spv
|   |       |   |   |-- blend_subtract.frag
|   |       |   |   |-- blend_subtract.spv
|   |       |   |   |-- blur.frag
|   |       |   |   |-- blur.spv
|   |       |   |   |-- bounce.frag
|   |       |   |   |-- bounce.spv
|   |       |   |   |-- bulge.frag
|   |       |   |   |-- bulge.spv
|   |       |   |   |-- camera3d.frag
|   |       |   |   |-- camera3d.spv
|   |       |   |   |-- camera3d.vert
|   |       |   |   |-- camera_shake.frag
|   |       |   |   |-- camera_shake.spv
|   |       |   |   |-- camera_shake_pro.frag
|   |       |   |   |-- camera_shake_pro.spv
|   |       |   |   |-- chroma_key.frag
|   |       |   |   |-- chroma_key.spv
|   |       |   |   |-- chroma_key.vert
|   |       |   |   |-- chromatic_aberration.frag
|   |       |   |   |-- chromatic_aberration.spv
|   |       |   |   |-- color_correction.frag
|   |       |   |   |-- composite.frag
|   |       |   |   |-- composite.spv
|   |       |   |   |-- corner_pin.comp
|   |       |   |   |-- corner_pin.spv
|   |       |   |   |-- crt.frag
|   |       |   |   |-- crt.spv
|   |       |   |   |-- depth_of_field.frag
|   |       |   |   |-- depth_of_field.spv
|   |       |   |   |-- depth_of_field.vert
|   |       |   |   |-- directional_blur.frag
|   |       |   |   |-- directional_blur.spv
|   |       |   |   |-- directional_blur.vert
|   |       |   |   |-- displacement_map.frag
|   |       |   |   |-- displacement_map.spv
|   |       |   |   |-- drift.frag
|   |       |   |   |-- drift.spv
|   |       |   |   |-- dynamic_zoom.frag
|   |       |   |   |-- dynamic_zoom.spv
|   |       |   |   |-- elastic.frag
|   |       |   |   |-- elastic.spv
|   |       |   |   |-- film_damage.frag
|   |       |   |   |-- film_damage.spv
|   |       |   |   |-- film_grain.frag
|   |       |   |   |-- film_grain.spv
|   |       |   |   |-- frame_blend.frag
|   |       |   |   |-- frame_blend.spv
|   |       |   |   |-- fullscreen.spv
|   |       |   |   |-- fullscreen.vert
|   |       |   |   |-- glitch.frag
|   |       |   |   |-- glitch.spv
|   |       |   |   |-- jitter.frag
|   |       |   |   |-- jitter.spv
|   |       |   |   |-- letterbox.frag
|   |       |   |   |-- lut.frag
|   |       |   |   |-- mask.frag
|   |       |   |   |-- mask.spv
|   |       |   |   |-- mesh_pbr.frag
|   |       |   |   |-- mesh_pbr.vert
|   |       |   |   |-- mesh_warp.frag
|   |       |   |   |-- mesh_warp.spv
|   |       |   |   |-- motion_blur.frag
|   |       |   |   |-- motion_blur.spv
|   |       |   |   |-- motion_blur.vert
|   |       |   |   |-- motion_transform.spv
|   |       |   |   |-- motion_transform.vert
|   |       |   |   |-- null_layer.frag
|   |       |   |   |-- null_layer.spv
|   |       |   |   |-- null_layer.vert
|   |       |   |   |-- optical_flow.comp
|   |       |   |   |-- optical_flow_pyramid.comp
|   |       |   |   |-- orbit.frag
|   |       |   |   |-- orbit.spv
|   |       |   |   |-- oscillate.frag
|   |       |   |   |-- oscillate.spv
|   |       |   |   |-- output.frag
|   |       |   |   |-- output.spv
|   |       |   |   |-- output.vert
|   |       |   |   |-- particle.frag
|   |       |   |   |-- particle.spv
|   |       |   |   |-- particle.vert
|   |       |   |   |-- particle_sim.comp
|   |       |   |   |-- planar_tracker.comp
|   |       |   |   |-- planar_tracker.spv
|   |       |   |   |-- polar_coordinates.frag
|   |       |   |   |-- polar_coordinates.spv
|   |       |   |   |-- posterize_time.frag
|   |       |   |   |-- posterize_time.spv
|   |       |   |   |-- pulse.frag
|   |       |   |   |-- pulse.spv
|   |       |   |   |-- radial_blur.frag
|   |       |   |   |-- radial_blur.spv
|   |       |   |   |-- random_displacement.frag
|   |       |   |   |-- random_displacement.spv
|   |       |   |   |-- rgb_shift.frag
|   |       |   |   |-- rgb_shift.spv
|   |       |   |   |-- ripple.frag
|   |       |   |   |-- ripple.spv
|   |       |   |   |-- scanlines.frag
|   |       |   |   |-- scanlines.spv
|   |       |   |   |-- shake.frag
|   |       |   |   |-- shake.spv
|   |       |   |   |-- shape2d.frag
|   |       |   |   |-- shape2d.spv
|   |       |   |   |-- shape2d.vert
|   |       |   |   |-- shape2d_tessellate.comp
|   |       |   |   |-- shape_merge.frag
|   |       |   |   |-- shape_merge.spv
|   |       |   |   |-- shape_transform.spv
|   |       |   |   |-- shape_transform.vert
|   |       |   |   |-- stabilize.comp
|   |       |   |   |-- stabilize.spv
|   |       |   |   |-- stop_motion.frag
|   |       |   |   |-- stop_motion.spv
|   |       |   |   |-- stroke_source.frag
|   |       |   |   |-- stroke_source.spv
|   |       |   |   |-- stroke_source.vert
|   |       |   |   |-- swing.f