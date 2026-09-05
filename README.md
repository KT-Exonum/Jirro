# VFX Engine — Android Node-Based Video/VFX Editor

A desktop-grade, node-based VFX + video editing engine for Android:
Kotlin/Compose UI ⇄ (async JNI) ⇄ C++23 engine ⇄ Vulkan (primary) / GLES 3.2 (fallback).

This repo is deliberately built **phase by phase**, per the project spec. Building all
subsystems at once produces something nobody can debug. Each phase below is a
self-contained, testable milestone, and the abstractions are designed so a
team can work on different phases in parallel without stepping on each other.

## What's implemented now vs. scaffolded

| Phase | Status | Where |
|---|---|---|
| 1. Native rendering sandbox (Vulkan triangle) | **Fully implemented** | `engine/vulkan/VulkanDevice.*` |
| 2. GPU video pipeline (AMediaCodec → Vulkan image) | **Fully implemented** — real `AMediaExtractor`/`AMediaCodec`/`AImageReader` decode, zero-copy `AHardwareBuffer` → `VkImage` import with `VK_KHR_sampler_ycbcr_conversion`, decoder pool + frame cache, dedicated media thread | `engine/media/*`, `engine/vulkan/VulkanDevice.cpp` (`ImportHardwareBuffer`, `CreateTexture`) |
| 3. Render graph & compositing | Data structures + dependency resolution/topo-sort implemented; VideoSource pass execution wired to MediaEngine (Phase 2); Shader/Blend/Composite/Output pass execution stubbed pending pipeline work | `engine/graph/*` |
| 4. Timeline & keyframes | **Fully implemented** (pure logic, no GPU dependency) | `engine/timeline/*` |
| 5. Compose frontend | Minimal scaffold (SurfaceView host + command bridge), not the full editor UI | `app/src/main/java/.../ui/*` |
| 6. Optimization & export | Hooks only (`GraphicsDevice::GetLastFrameStats`, pool classes) | throughout |

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
  Phase 3 should thread per-node source time through `CompiledPass` instead
  of relying on the caller having pre-resolved it, once Shader/Blend passes
  need `timelineSeconds` for their own keyframe evaluation *and* a
  VideoSource ancestor needs source time simultaneously.
- `DecoderPool`'s eviction policy is plain LRU; Section 8 leaves room for a
  smarter policy (e.g. weighting by proximity to active transitions), not
  needed until Phase 3's transition/composite work exists to exercise it.
- `FrameCache::EvictOutsideWindow` approximates the per-clip frame duration
  (24fps) rather than reading each clip's actual frame rate; harmless for
  correctness (it only affects memory-window sizing) but should be replaced
  once `VideoDecoder` exposes the source's frame rate from `AMediaFormat`.
- GLES fallback (`OpenGLDevice`) does not yet have a hardware-buffer import
  path (`EGL_ANDROID_get_native_client_buffer` + `GL_TEXTURE_EXTERNAL_OES`);
  Phase 2's video pipeline currently requires the Vulkan backend.

Rationale for what's still stubbed in Phase 3: full render-pass execution
for Shader/Blend/Composite/Output needs pipeline + descriptor-set plumbing
that's best developed once there's a real fragment shader to bind (Section
3), which is Phase 3's job per the spec's phase breakdown.

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
    Node.h                  - Node/Connection/Resource abstractions (+ sourceFilePath
                              for VideoSource nodes, Phase 2)
    RenderGraph.h/.cpp      - dependency resolution, topological execution order,
                              resource-lifetime + pooling hooks, VideoSource pass
                              execution wired to MediaEngine (Phase 2)
  engine/timeline/
    Keyframe.h              - KeyframeTrack, interpolation (linear/step/bezier/custom)
    Timeline.h               - TimelineTime, deterministic Render(time) contract
  engine/media/
    MediaEngine.h/.cpp       - Phase 2: VideoDecoder (AMediaExtractor/AMediaCodec/
                               AImageReader), DecoderPool (bounded, LRU-evicted),
                               FrameCache (windowed around the playhead, mutex-guarded
                               for cross-thread reads), and the MediaEngine facade that
                               owns the dedicated media thread
```
