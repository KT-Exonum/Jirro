#include "RenderGraph.h"

#include <android/log.h>

#include <algorithm>
#include <array>
#include <deque>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "engine/audio/AudioEngine.h"

#define LOG_TAG "RenderGraph"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vfx {

// Forward declare SPIR-V bytecode from VulkanDevice.cpp (generated_shader_bytecode namespace)
extern const uint32_t* kFullscreenVertSpirv;
extern size_t kFullscreenVertSpirvWords;
extern const uint32_t* kBlendNormalFragSpirv;
extern size_t kBlendNormalFragSpirvWords;
extern const uint32_t* kBlendMultiplyFragSpirv;
extern size_t kBlendMultiplyFragSpirvWords;
extern const uint32_t* kBlendScreenFragSpirv;
extern size_t kBlendScreenFragSpirvWords;
extern const uint32_t* kBlendOverlayFragSpirv;
extern size_t kBlendOverlayFragSpirvWords;
extern const uint32_t* kBlendAddFragSpirv;
extern size_t kBlendAddFragSpirvWords;
extern const uint32_t* kBlendSubtractFragSpirv;
extern size_t kBlendSubtractFragSpirvWords;
extern const uint32_t* kColorCorrectionFragSpirv;
extern size_t kColorCorrectionFragSpirvWords;
extern const uint32_t* kBlurFragSpirv;
extern size_t kBlurFragSpirvWords;
extern const uint32_t* kMaskFragSpirv;
extern size_t kMaskFragSpirvWords;
extern const uint32_t* kCompositeFragSpirv;
extern size_t kCompositeFragSpirvWords;
extern const uint32_t* kVectorSourceVertSpirv;
extern size_t kVectorSourceVertSpirvWords;
extern const uint32_t* kVectorSourceFragSpirv;
extern size_t kVectorSourceFragSpirvWords;
extern const uint32_t* kTextSourceVertSpirv;
extern size_t kTextSourceVertSpirvWords;
extern const uint32_t* kTextSourceFragSpirv;
extern size_t kTextSourceFragSpirvWords;
extern const uint32_t* kStrokeSourceVertSpirv;
extern size_t kStrokeSourceVertSpirvWords;
extern const uint32_t* kStrokeSourceFragSpirv;
extern size_t kStrokeSourceFragSpirvWords;
extern const uint32_t* kAdjustmentVertSpirv;
extern size_t kAdjustmentVertSpirvWords;
extern const uint32_t* kAdjustmentFragSpirv;
extern size_t kAdjustmentFragSpirvWords;
extern const uint32_t* kNullLayerVertSpirv;
extern size_t kNullLayerVertSpirvWords;
extern const uint32_t* kNullLayerFragSpirv;
extern size_t kNullLayerFragSpirvWords;
extern const uint32_t* kOutputVertSpirv;
extern size_t kOutputVertSpirvWords;
extern const uint32_t* kOutputFragSpirv;
extern size_t kOutputFragSpirvWords;
extern const uint32_t* kMotionBlurVertSpirv;
extern size_t kMotionBlurVertSpirvWords;
extern const uint32_t* kMotionBlurFragSpirv;
extern size_t kMotionBlurFragSpirvWords;
extern const uint32_t* kDirectionalBlurVertSpirv;
extern size_t kDirectionalBlurVertSpirvWords;
extern const uint32_t* kDirectionalBlurFragSpirv;
extern size_t kDirectionalBlurFragSpirvWords;
extern const uint32_t* kTimeRemapVertSpirv;
extern size_t kTimeRemapVertSpirvWords;
extern const uint32_t* kTimeRemapFragSpirv;
extern size_t kTimeRemapFragSpirvWords;
extern const uint32_t* kBezierMaskVertSpirv;
extern size_t kBezierMaskVertSpirvWords;
extern const uint32_t* kBezierMaskFragSpirv;
extern size_t kBezierMaskFragSpirvWords;
extern const uint32_t* kParticleVertSpirv;
extern size_t kParticleVertSpirvWords;
extern const uint32_t* kParticleFragSpirv;
extern size_t kParticleFragSpirvWords;
extern const uint32_t* kParticleSimCompSpirv;
extern size_t kParticleSimCompSpirvWords;
extern const uint32_t* kShape2DVertSpirv;
extern size_t kShape2DVertSpirvWords;
extern const uint32_t* kShape2DFragSpirv;
extern size_t kShape2DFragSpirvWords;
extern const uint32_t* kShapeMergeFragSpirv;
extern size_t kShapeMergeFragSpirvWords;
extern const uint32_t* kShapeTransformVertSpirv;
extern size_t kShapeTransformVertSpirvWords;
extern const uint32_t* kTransform3DVertSpirv;
extern size_t kTransform3DVertSpirvWords;
extern const uint32_t* kTransform3DFragSpirv;
extern size_t kTransform3DFragSpirvWords;
extern const uint32_t* kCamera3DVertSpirv;
extern size_t kCamera3DVertSpirvWords;
extern const uint32_t* kDepthOfFieldFragSpirv;
extern size_t kDepthOfFieldFragSpirvWords;
extern const uint32_t* kChromaKeyVertSpirv;
extern size_t kChromaKeyVertSpirvWords;
extern const uint32_t* kChromaKeyFragSpirv;
extern size_t kChromaKeyFragSpirvWords;
extern const uint32_t* kMeshPBRVertSpirv;
extern size_t kMeshPBRVertSpirvWords;
extern const uint32_t* kMeshPBRFragSpirv;
extern size_t kMeshPBRFragSpirvWords;

// Motion transform vertex shader (shared by all motion effects)
extern const uint32_t* kMotionTransformVertSpirv;
extern size_t kMotionTransformVertSpirvWords;

// Motion Effects shaders
extern const uint32_t* kOscillateFragSpirv;
extern size_t kOscillateFragSpirvWords;
extern const uint32_t* kShakeFragSpirv;
extern size_t kShakeFragSpirvWords;
extern const uint32_t* kRandomDisplacementFragSpirv;
extern size_t kRandomDisplacementFragSpirvWords;
extern const uint32_t* kPulseFragSpirv;
extern size_t kPulseFragSpirvWords;
extern const uint32_t* kSwingFragSpirv;
extern size_t kSwingFragSpirvWords;
extern const uint32_t* kBounceFragSpirv;
extern size_t kBounceFragSpirvWords;
extern const uint32_t* kElasticFragSpirv;
extern size_t kElasticFragSpirvWords;
extern const uint32_t* kCameraShakeFragSpirv;
extern size_t kCameraShakeFragSpirvWords;
extern const uint32_t* kZoomBlurFragSpirv;
extern size_t kZoomBlurFragSpirvWords;
extern const uint32_t* kRadialBlurFragSpirv;
extern size_t kRadialBlurFragSpirvWords;
extern const uint32_t* kRippleFragSpirv;
extern size_t kRippleFragSpirvWords;
extern const uint32_t* kWaveFragSpirv;
extern size_t kWaveFragSpirvWords;
extern const uint32_t* kTwistFragSpirv;
extern size_t kTwistFragSpirvWords;
extern const uint32_t* kBulgeFragSpirv;
extern size_t kBulgeFragSpirvWords;
extern const uint32_t* kVortexFragSpirv;
extern size_t kVortexFragSpirvWords;
extern const uint32_t* kGlitchFragSpirv;
extern size_t kGlitchFragSpirvWords;
extern const uint32_t* kVHSFragSpirv;
extern size_t kVHSFragSpirvWords;
extern const uint32_t* kScanlinesFragSpirv;
extern size_t kScanlinesFragSpirvWords;
extern const uint32_t* kCRTFragSpirv;
extern size_t kCRTFragSpirvWords;
extern const uint32_t* kChromaticAberrationFragSpirv;
extern size_t kChromaticAberrationFragSpirvWords;
extern const uint32_t* kRGBShiftFragSpirv;
extern size_t kRGBShiftFragSpirvWords;
extern const uint32_t* kTimeStretchFragSpirv;
extern size_t kTimeStretchFragSpirvWords;
extern const uint32_t* kFrameBlendFragSpirv;
extern size_t kFrameBlendFragSpirvWords;
extern const uint32_t* kStopMotionFragSpirv;
extern size_t kStopMotionFragSpirvWords;
extern const uint32_t* kPosterizeTimeFragSpirv;
extern size_t kPosterizeTimeFragSpirvWords;
extern const uint32_t* kWiggleFragSpirv;
extern size_t kWiggleFragSpirvWords;
extern const uint32_t* kJitterFragSpirv;
extern size_t kJitterFragSpirvWords;
extern const uint32_t* kDriftFragSpirv;
extern size_t kDriftFragSpirvWords;
extern const uint32_t* kOrbitFragSpirv;
extern size_t kOrbitFragSpirvWords;
extern const uint32_t* kCameraShakeProFragSpirv;
extern size_t kCameraShakeProFragSpirvWords;
extern const uint32_t* kDynamicZoomFragSpirv;
extern size_t kDynamicZoomFragSpirvWords;
extern const uint32_t* kFilmDamageFragSpirv;
extern size_t kFilmDamageFragSpirvWords;
extern const uint32_t* kFilmGrainFragSpirv;
extern size_t kFilmGrainFragSpirvWords;
extern const uint32_t* kVignetteFragSpirv;
extern size_t kVignetteFragSpirvWords;
extern const uint32_t* kLetterboxFragSpirv;
extern size_t kLetterboxFragSpirvWords;
extern const uint32_t* kBezierWarpFragSpirv;
extern size_t kBezierWarpFragSpirvWords;
extern const uint32_t* kMeshWarpFragSpirv;
extern size_t kMeshWarpFragSpirvWords;
extern const uint32_t* kPolarCoordinatesFragSpirv;
extern size_t kPolarCoordinatesFragSpirvWords;
extern const uint32_t* kDisplacementMapFragSpirv;
extern size_t kDisplacementMapFragSpirvWords;

// Audio visualization shaders
extern const uint32_t* kAudioReactiveFragSpirv;
extern size_t kAudioReactiveFragSpirvWords;
extern const uint32_t* kAudioWaveformFragSpirv;
extern size_t kAudioWaveformFragSpirvWords;
extern const uint32_t* kAudioSpectrumFragSpirv;
extern size_t kAudioSpectrumFragSpirvWords;

// Text shaders
extern const uint32_t* kTextVertSpirv;
extern size_t kTextVertSpirvWords;
extern const uint32_t* kTextFragSpirv;
extern size_t kTextFragSpirvWords;
extern const uint32_t* kTextAnimatorVertSpirv;
extern size_t kTextAnimatorVertSpirvWords;
extern const uint32_t* kTextPathVertSpirv;
extern size_t kTextPathVertSpirvWords;

// ---------------------------------------------------------------------------
// Shader registry: replaces the old 200+ line switch(pass.kind) with a
// data-driven lookup. Each NodeKind maps to a vertex/fragment SPIR-V pair.
// Blend nodes have a secondary lookup by BlendMode.
// ---------------------------------------------------------------------------

struct ShaderEntry {
    const uint32_t* vert;
    size_t vertWords;
    const uint32_t* frag;
    size_t fragWords;
};

struct NodeKindHash {
    auto operator()(NodeKind k) const noexcept {
        return static_cast<std::underlying_type_t<NodeKind>>(k);
    }
};

static const std::unordered_map<NodeKind, ShaderEntry, NodeKindHash>& GetShaderRegistry() {
    static const std::unordered_map<NodeKind, ShaderEntry, NodeKindHash> registry = []{
        std::unordered_map<NodeKind, ShaderEntry, NodeKindHash> m;
        #define V(kind, vs, vsW, fs, fsW) m[NodeKind::kind] = {vs, vsW, fs, fsW}
        // Sources
        V(ImageSource, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        V(AudioSource, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        V(VectorSource, kVectorSourceVertSpirv, kVectorSourceVertSpirvWords, kVectorSourceFragSpirv, kVectorSourceFragSpirvWords);
        V(TextSource, kTextVertSpirv, kTextVertSpirvWords, kTextFragSpirv, kTextFragSpirvWords);
        V(StrokeSource, kStrokeSourceVertSpirv, kStrokeSourceVertSpirvWords, kStrokeSourceFragSpirv, kStrokeSourceFragSpirvWords);
        // Text animation
        V(TextAnimator, kTextAnimatorVertSpirv, kTextAnimatorVertSpirvWords, kTextFragSpirv, kTextFragSpirvWords);
        V(TextPath, kTextPathVertSpirv, kTextPathVertSpirvWords, kTextFragSpirv, kTextFragSpirvWords);
        V(Typewriter, kTextAnimatorVertSpirv, kTextAnimatorVertSpirvWords, kTextFragSpirv, kTextFragSpirvWords);
        // Effects
        V(Shader, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        V(ColorCorrection, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kColorCorrectionFragSpirv, kColorCorrectionFragSpirvWords);
        V(Blur, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kBlurFragSpirv, kBlurFragSpirvWords);
        V(Mask, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kMaskFragSpirv, kMaskFragSpirvWords);
        V(Composite, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kCompositeFragSpirv, kCompositeFragSpirvWords);
        V(Adjustment, kAdjustmentVertSpirv, kAdjustmentVertSpirvWords, kAdjustmentFragSpirv, kAdjustmentFragSpirvWords);
        // Output
        V(Output, kOutputVertSpirv, kOutputVertSpirvWords, kOutputFragSpirv, kOutputFragSpirvWords);
        // Null
        V(Null, kNullLayerVertSpirv, kNullLayerVertSpirvWords, kNullLayerFragSpirv, kNullLayerFragSpirvWords);
        // Motion blur
        V(MotionBlur, kMotionBlurVertSpirv, kMotionBlurVertSpirvWords, kMotionBlurFragSpirv, kMotionBlurFragSpirvWords);
        V(DirectionalBlur, kDirectionalBlurVertSpirv, kDirectionalBlurVertSpirvWords, kDirectionalBlurFragSpirv, kDirectionalBlurFragSpirvWords);
        V(TransformBlur, kDirectionalBlurVertSpirv, kDirectionalBlurVertSpirvWords, kDirectionalBlurFragSpirv, kDirectionalBlurFragSpirvWords);
        // Time remap
        V(VelocityGraph, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kTimeRemapFragSpirv, kTimeRemapFragSpirvWords);
        V(TimeRemap, kTimeRemapVertSpirv, kTimeRemapVertSpirvWords, kTimeRemapFragSpirv, kTimeRemapFragSpirvWords);
        V(OpticalFlow, kTimeRemapVertSpirv, kTimeRemapVertSpirvWords, kTimeRemapFragSpirv, kTimeRemapFragSpirvWords);
        // Masking
        V(BezierMask, kBezierMaskVertSpirv, kBezierMaskVertSpirvWords, kBezierMaskFragSpirv, kBezierMaskFragSpirvWords);
        V(Rotoscoping, kBezierMaskVertSpirv, kBezierMaskVertSpirvWords, kBezierMaskFragSpirv, kBezierMaskFragSpirvWords);
        V(RotoBrush, kBezierMaskVertSpirv, kBezierMaskVertSpirvWords, kBezierMaskFragSpirv, kBezierMaskFragSpirvWords);
        V(Tracker, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        // Particles
        V(ParticleEmitter, kParticleVertSpirv, kParticleVertSpirvWords, kParticleFragSpirv, kParticleFragSpirvWords);
        V(ParticleForces, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        V(ParticleRenderer, kParticleVertSpirv, kParticleVertSpirvWords, kParticleFragSpirv, kParticleFragSpirvWords);
        // Shape2D
        V(ShapeRectangle, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapeEllipse, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapePolygon, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapeStar, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapePath, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapeRender, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapeMerge, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kShapeMergeFragSpirv, kShapeMergeFragSpirvWords);
        V(ShapeTransform, kShapeTransformVertSpirv, kShapeTransformVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        V(ShapeStroke, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapeFill, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapeRepeater, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapeBoolean, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        // 2.5D
        V(Transform3D, kTransform3DVertSpirv, kTransform3DVertSpirvWords, kTransform3DFragSpirv, kTransform3DFragSpirvWords);
        V(Camera3D, kCamera3DVertSpirv, kCamera3DVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        V(DepthOfField, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kDepthOfFieldFragSpirv, kDepthOfFieldFragSpirvWords);
        // Keying
        V(ChromaKey, kChromaKeyVertSpirv, kChromaKeyVertSpirvWords, kChromaKeyFragSpirv, kChromaKeyFragSpirvWords);
        // 3D
        V(MeshSource, kMeshPBRVertSpirv, kMeshPBRVertSpirvWords, kMeshPBRFragSpirv, kMeshPBRFragSpirvWords);
        V(Group, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        // Motion Effects - Transform Motion
        V(Oscillate, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kOscillateFragSpirv, kOscillateFragSpirvWords);
        V(Shake, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kShakeFragSpirv, kShakeFragSpirvWords);
        V(RandomDisplacement, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kRandomDisplacementFragSpirv, kRandomDisplacementFragSpirvWords);
        V(Pulse, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kPulseFragSpirv, kPulseFragSpirvWords);
        V(Swing, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kSwingFragSpirv, kSwingFragSpirvWords);
        V(Bounce, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kBounceFragSpirv, kBounceFragSpirvWords);
        V(Elastic, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kElasticFragSpirv, kElasticFragSpirvWords);
        // Motion Effects - Camera Motion
        V(CameraShake, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kCameraShakeFragSpirv, kCameraShakeFragSpirvWords);
        V(ZoomBlur, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kZoomBlurFragSpirv, kZoomBlurFragSpirvWords);
        V(RadialBlur, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kRadialBlurFragSpirv, kRadialBlurFragSpirvWords);
        V(MotionBlur, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kMotionBlurFragSpirv, kMotionBlurFragSpirvWords);
        V(DirectionalBlur, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kDirectionalBlurFragSpirv, kDirectionalBlurFragSpirvWords);
        // Motion Effects - Distortion Motion
        V(Ripple, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kRippleFragSpirv, kRippleFragSpirvWords);
        V(Wave, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kWaveFragSpirv, kWaveFragSpirvWords);
        V(Twist, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kTwistFragSpirv, kTwistFragSpirvWords);
        V(Bulge, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kBulgeFragSpirv, kBulgeFragSpirvWords);
        V(Vortex, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kVortexFragSpirv, kVortexFragSpirvWords);
        // Motion Effects - Stylize Motion
        V(Glitch, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kGlitchFragSpirv, kGlitchFragSpirvWords);
        V(VHS, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kVHSFragSpirv, kVHSFragSpirvWords);
        V(Scanlines, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kScanlinesFragSpirv, kScanlinesFragSpirvWords);
        V(CRT, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kCRTFragSpirv, kCRTFragSpirvWords);
        V(ChromaticAberration, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kChromaticAberrationFragSpirv, kChromaticAberrationFragSpirvWords);
        V(RGBShift, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kRGBShiftFragSpirv, kRGBShiftFragSpirvWords);
        // Motion Effects - Time Motion
        V(TimeStretch, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kTimeStretchFragSpirv, kTimeStretchFragSpirvWords);
        V(FrameBlend, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kFrameBlendFragSpirv, kFrameBlendFragSpirvWords);
        V(StopMotion, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kStopMotionFragSpirv, kStopMotionFragSpirvWords);
        V(PosterizeTime, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kPosterizeTimeFragSpirv, kPosterizeTimeFragSpirvWords);
        // Motion Effects - Utility Motion
        V(Wiggle, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kWiggleFragSpirv, kWiggleFragSpirvWords);
        V(Jitter, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kJitterFragSpirv, kJitterFragSpirvWords);
        V(Drift, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kDriftFragSpirv, kDriftFragSpirvWords);
        V(Orbit, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kOrbitFragSpirv, kOrbitFragSpirvWords);
        // Resolve FX inspired
        V(CameraShakePro, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kCameraShakeProFragSpirv, kCameraShakeProFragSpirvWords);
        V(DynamicZoom, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kDynamicZoomFragSpirv, kDynamicZoomFragSpirvWords);
        V(FilmDamage, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kFilmDamageFragSpirv, kFilmDamageFragSpirvWords);
        V(FilmGrain, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kFilmGrainFragSpirv, kFilmGrainFragSpirvWords);
        V(Vignette, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kVignetteFragSpirv, kVignetteFragSpirvWords);
        V(Letterbox, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kLetterboxFragSpirv, kLetterboxFragSpirvWords);
        // Advanced
        V(BezierWarp, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kBezierWarpFragSpirv, kBezierWarpFragSpirvWords);
        V(MeshWarp, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kMeshWarpFragSpirv, kMeshWarpFragSpirvWords);
        V(PolarCoordinates, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kPolarCoordinatesFragSpirv, kPolarCoordinatesFragSpirvWords);
        V(DisplacementMap, kMotionTransformVertSpirv, kMotionTransformVertSpirvWords, kDisplacementMapFragSpirv, kDisplacementMapFragSpirvWords);
        // Audio visualization
        V(AudioReactive, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kAudioReactiveFragSpirv, kAudioReactiveFragSpirvWords);
        V(AudioWaveform, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kAudioWaveformFragSpirv, kAudioWaveformFragSpirvWords);
        V(AudioSpectrum, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kAudioSpectrumFragSpirv, kAudioSpectrumFragSpirvWords);
        #undef V
        return m;
    }();
    return registry;
}

static const std::unordered_map<BlendMode, std::pair<const uint32_t*, size_t>>& GetBlendFragRegistry() {
    static const std::unordered_map<BlendMode, std::pair<const uint32_t*, size_t>> registry = []{
        std::unordered_map<BlendMode, std::pair<const uint32_t*, size_t>> m;
        m[BlendMode::Normal] = {kBlendNormalFragSpirv, kBlendNormalFragSpirvWords};
        m[BlendMode::Multiply] = {kBlendMultiplyFragSpirv, kBlendMultiplyFragSpirvWords};
        m[BlendMode::Screen] = {kBlendScreenFragSpirv, kBlendScreenFragSpirvWords};
        m[BlendMode::Overlay] = {kBlendOverlayFragSpirv, kBlendOverlayFragSpirvWords};
        m[BlendMode::Add] = {kBlendAddFragSpirv, kBlendAddFragSpirvWords};
        m[BlendMode::Subtract] = {kBlendSubtractFragSpirv, kBlendSubtractFragSpirvWords};
        return m;
    }();
    return registry;
}

static std::optional<ShaderEntry> ResolveShaderEntry(NodeKind kind, BlendMode blendMode) {
    if (kind == NodeKind::Blend) {
        const auto& blendRegistry = GetBlendFragRegistry();
        auto it = blendRegistry.find(blendMode);
        if (it != blendRegistry.end()) {
            return ShaderEntry{kFullscreenVertSpirv, kFullscreenVertSpirvWords, it->second.first, it->second.second};
        }
        return std::nullopt;
    }

    const auto& registry = GetShaderRegistry();
    auto it = registry.find(kind);
    if (it != registry.end()) {
        return it->second;
    }
    return std::nullopt;
}

CompileResult RenderGraph::Compile(const NodeGraph& graph, const std::string& outputNodeId) {
    CompileResult result;

    const Node* outputNode = graph.FindNode(outputNodeId);
    if (!outputNode) {
        result.cycleNodeIds = {outputNodeId};
        LOGE("Compile: output node '%s' not found", outputNodeId.c_str());
        return result;
    }

    std::unordered_set<std::string> reachable;
    std::deque<std::string> toVisit{outputNodeId};
    while (!toVisit.empty()) {
        std::string id = toVisit.front();
        toVisit.pop_front();
        if (!reachable.insert(id).second) continue;
        for (const Connection* c : graph.InputsTo(id)) toVisit.push_back(c->fromNodeId);
    }

    // Expand groups: recursively inline group members into the main graph
    ExpandGroups(graph, reachable);

    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> dependents;
    for (const auto& id : reachable) inDegree[id] = 0;

    for (const auto& conn : graph.AllConnections()) {
        if (!reachable.count(conn.fromNodeId) || !reachable.count(conn.toNodeId)) continue;
        inDegree[conn.toNodeId]++;
        dependents[conn.fromNodeId].push_back(conn.toNodeId);
    }

    std::deque<std::string> ready;
    for (const auto& [id, deg] : inDegree) if (deg == 0) ready.push_back(id);

    std::vector<std::string> order;
    while (!ready.empty()) {
        std::sort(ready.begin(), ready.end());
        std::string current = ready.front();
        ready.pop_front();
        order.push_back(current);

        for (const auto& dep : dependents[current]) {
            if (--inDegree[dep] == 0) ready.push_back(dep);
        }
    }

    if (order.size() != reachable.size()) {
        for (const auto& [id, deg] : inDegree) if (deg > 0) result.cycleNodeIds.push_back(id);
        LOGE("Compile: cycle detected involving %zu node(s)", result.cycleNodeIds.size());
        return result;
    }

    for (const auto& id : order) {
        const Node* node = graph.FindNode(id);
        CompiledPass pass;
        pass.nodeId = id;
        pass.kind = node->kind;
        pass.isFinalOutput = (id == outputNodeId);
        for (const Connection* c : graph.InputsTo(id)) pass.inputNodeIds.push_back(c->fromNodeId);
        
        // Mark particle nodes
        if (node->kind == NodeKind::ParticleEmitter || 
            node->kind == NodeKind::ParticleForces || 
            node->kind == NodeKind::ParticleRenderer) {
            pass.isParticleNode = true;
            if (node->kind == NodeKind::ParticleEmitter || 
                node->kind == NodeKind::ParticleForces) {
                // These run compute shaders for GPU particles
                pass.isParticleCompute = node->particle.useGpuParticles;
            }
        }
        
        result.passes.push_back(std::move(pass));
    }

    return result;
}

// Expand groups by inlining their member nodes into the main graph
void RenderGraph::ExpandGroups(const NodeGraph& graph, std::unordered_set<std::string>& reachable) {
    // Collect all groups in the reachable subgraph
    std::vector<std::string> groupNodes;
    for (const auto& id : reachable) {
        const Node* node = graph.FindNode(id);
        if (node && node->kind == NodeKind::Group) {
            groupNodes.push_back(id);
        }
    }

    // For each group, inline its members
    for (const auto& groupId : groupNodes) {
        const Node* groupNode = graph.FindNode(groupId);
        if (!groupNode) continue;

        // Get the group info
        const auto* group = graph.FindGroup(groupNode->groupId);
        if (!group) continue;

        // Collect all members of this group (recursively for nested groups)
        std::vector<std::string> groupMembers = graph.GetGroupMembers(groupNode->groupId);
        
        // Build mapping from old member IDs to new IDs (with prefix to avoid conflicts)
        std::unordered_map<std::string, std::string> idMap;
        for (const auto& memberId : groupMembers) {
            std::string newId = groupId + "_" + memberId;
            idMap[memberId] = newId;
        }

        // Note: Full group expansion would require modifying the graph structure
        // which is complex since the graph is passed as const.
        // For now, we mark group nodes to be skipped during execution.
        // A full implementation would require:
        // 1. Creating new node IDs for inlined members
        // 2. Remapping all connections (internal and external)
        // 3. Exposing group inputs/outputs as connections to/from group boundary
        // 3. Removing the group node from the execution plan
        
        // For now, we just remove the group node from reachable so it won't execute
        reachable.erase(groupNode->nodeId);
        
        // In a full implementation, we would:
        // 1. Create new nodes for each member with new IDs
        // 2. Remap internal connections
        // 3. Connect external inputs to group's exposed inputs
        // 4. Connect group's exposed outputs to external outputs
        // 5. Add the new nodes to the reachable set
    }
}

void RenderGraph::Execute(const NodeGraph& graph, const CompileResult& plan, double timelineSeconds,
                           MediaEngine* mediaEngine,
                           ExpressionEngine* expressionEngine) {
    if (!plan.Ok()) {
        LOGE("Execute called on a plan that failed to compile — aborting frame");
        return;
    }
    
    // Check if we need to initialize particle system
    for (const auto& pass : plan.passes) {
        if (pass.isParticleNode && !particleState_.initialized) {
            // Find the particle config from the first particle node
            const Node* node = graph.FindNode(pass.nodeId);
            if (node) {
                InitializeParticleSystem(node->particle);
                break;
            }
        }
    }
    
    double deltaTime = timelineSeconds - lastTimelineSeconds_;
    if (deltaTime <= 0.0) deltaTime = 1.0 / 60.0; // fallback for first frame
    lastTimelineSeconds_ = timelineSeconds;
    
    for (const auto& pass : plan.passes) {
        ExecutePass(graph, pass, timelineSeconds, mediaEngine, expressionEngine, audioEngine);
    }
    texturePool_.EndFrame();
}

ShaderModuleHandle RenderGraph::GetOrCreateShaderModule(const uint32_t* spirv, size_t wordCount) {
    std::string key(reinterpret_cast<const char*>(spirv), wordCount * sizeof(uint32_t));
    auto it = shaderModuleCache_.find(key);
    if (it != shaderModuleCache_.end()) return it->second;

    auto result = device_.CreateShaderModule(std::span<const uint32_t>(spirv, wordCount));
    if (!result) return ShaderModuleHandle{0, 0};

    ShaderModuleHandle handle = result.value;
    shaderModuleCache_[key] = handle;
    return handle;
}

void RenderGraph::ExecutePass(const NodeGraph& graph, const CompiledPass& pass, double timelineSeconds,
                                  MediaEngine* mediaEngine,
                                  ExpressionEngine* expressionEngine,
                                  AudioEngine* audioEngine) {
    const Node* node = graph.FindNode(pass.nodeId);
    if (!node) return;

    if (pass.kind == NodeKind::VideoSource) {
        if (mediaEngine) {
            if (auto frame = mediaEngine->TryGetFrame(pass.nodeId, timelineSeconds)) {
                lastVideoFrameByNode_[pass.nodeId] = frame->texture;
            }
        }
        return;
    }

    TextureDesc outputDesc;
    outputDesc.width = 1920;
    outputDesc.height = 1080;
    outputDesc.format = PixelFormat::RGBA8Unorm;
    outputDesc.usage = TextureUsage::ColorAttachmentAndSampled;
    outputDesc.transient = true;
    outputDesc.debugName = "pass_output_" + pass.nodeId;

    auto acquired = texturePool_.Acquire(outputDesc);
    if (!acquired) return;
    TextureHandle outputTexture = acquired.value;

    std::vector<TextureHandle> inputTextures;
    for (const std::string& inputNodeId : pass.inputNodeIds) {
        auto videoIt = lastVideoFrameByNode_.find(inputNodeId);
        if (videoIt != lastVideoFrameByNode_.end()) {
            inputTextures.push_back(videoIt->second);
        } else {
            LOGI("ExecutePass: input '%s' not found in lastVideoFrameByNode_", inputNodeId.c_str());
        }
    }

    ShaderModuleHandle vsHandle{0, 0};
    ShaderModuleHandle fsHandle{0, 0};

    // Special case: custom SPIR-V fragment shader
    if (pass.kind == NodeKind::Shader && !node->spirvFragment.empty()) {
        vsHandle = GetOrCreateShaderModule(kFullscreenVertSpirv, kFullscreenVertSpirvWords);
        fsHandle = GetOrCreateShaderModule(node->spirvFragment.data(), node->spirvFragment.size());
    } else {
        auto entry = ResolveShaderEntry(pass.kind, node->blendMode);
        if (!entry) {
            LOGE("ExecutePass: no shader entry for node '%s' kind=%d", pass.nodeId.c_str(), static_cast<int>(pass.kind));
            texturePool_.Release(outputTexture);
            return;
        }
        vsHandle = GetOrCreateShaderModule(entry->vert, entry->vertWords);
        fsHandle = GetOrCreateShaderModule(entry->frag, entry->fragWords);
    }

    if (!vsHandle.IsValid() || !fsHandle.IsValid()) {
        LOGE("ExecutePass: failed to get/create shader modules for node '%s'", pass.nodeId.c_str());
        texturePool_.Release(outputTexture);
        return;
    }

    auto pipelineResult = device_.GetOrCreatePipeline(vsHandle, fsHandle, outputDesc.usage);
    if (!pipelineResult) {
        LOGE("ExecutePass: failed to get/create pipeline for node '%s': %s", pass.nodeId.c_str(), pipelineResult.error.c_str());
        texturePool_.Release(outputTexture);
        return;
    }
    PipelineHandle pipelineHandle = pipelineResult.value;

    std::unordered_map<std::string, float> animatedUniforms;
    for (const auto& [name, track] : node->animatedUniforms) {
        if (!track.Empty()) {
            animatedUniforms[name] = track.Evaluate(timelineSeconds);
        }
    }
    
    // Evaluate expressions if ExpressionEngine is available
    if (expressionEngine) {
        ExpressionEngine::ExpressionContext ctx;
        ctx.time = timelineSeconds;
        ctx.frameRate = 30.0; // TODO: get from timeline
        ctx.frame = static_cast<int>(timelineSeconds * ctx.frameRate);
        
        for (const auto& [name, expr] : node->expressions) {
            if (expr.IsValid()) {
                auto result = expressionEngine->Evaluate(expr.script, ctx);
                if (result) {
                    animatedUniforms[name] = *result;
                }
            }
        }
    }
    
    // Inject audio reactive uniforms if audio engine is available
    if (audioEngine) {
        float audioLevel = 0.0f;
        if (node->kind == NodeKind::AudioReactive || node->kind == NodeKind::AudioWaveform || node->kind == NodeKind::AudioSpectrum) {
            audioLevel = audioEngine->GetAudioLevel(node->audio.sourceClipId);
        }
        animatedUniforms["audioLevel"] = audioLevel;
        animatedUniforms["beatPhase"] = audioEngine->GetBeatPhase(node->audio.sourceClipId);
        animatedUniforms["sensitivity"] = node->audio.sensitivity;
        animatedUniforms["smoothing"] = node->audio.smoothing;
        animatedUniforms["frequencyMin"] = node->audio.frequencyMin;
        animatedUniforms["frequencyMax"] = node->audio.frequencyMax;
        animatedUniforms["useBeatDetection"] = node->audio.useBeatDetection ? 1.0f : 0.0f;
        animatedUniforms["beatThreshold"] = node->audio.beatThreshold;
        animatedUniforms["waveformPoints"] = static_cast<float>(node->audio.waveformPoints);
        animatedUniforms["spectrumBars"] = static_cast<float>(node->audio.spectrumBars);
        animatedUniforms["barWidth"] = node->audio.barWidth;
        animatedUniforms["barGap"] = node->audio.barGap;
        animatedUniforms["barColorR"] = ((node->audio.barColor >> 16) & 0xFF) / 255.0f;
        animatedUniforms["barColorG"] = ((node->audio.barColor >> 8) & 0xFF) / 255.0f;
        animatedUniforms["barColorB"] = (node->audio.barColor & 0xFF) / 255.0f;
    }

    for (const auto& [name, value] : node->uniformFloats) {
        if (animatedUniforms.find(name) == animatedUniforms.end()) {
            animatedUniforms[name] = value;
        }
    }

    // Handle particle nodes
    if (pass.isParticleNode) {
        if (pass.isParticleCompute && node->particle.useGpuParticles) {
            // Compute pass for particle simulation
            double deltaTime = timelineSeconds - lastTimelineSeconds_;
            if (deltaTime < 0) deltaTime = 1.0 / 60.0; // fallback
            DispatchParticleCompute(pass, deltaTime, node->particle);
            // No output texture for compute pass
            return;
        } else if (node->kind == NodeKind::ParticleRenderer) {
            // Render particles
            ParticleDrawParams drawParams{};
            drawParams.particleBuffer = particleState_.particleBuffer;
            drawParams.simParamsBuffer = particleState_.simParamsBuffer;
            drawParams.indirectBuffer = particleState_.indirectBuffer;
            drawParams.maxParticles = particleState_.maxParticles;
            drawParams.pointSizeScale = 1.0f;
            drawParams.additiveBlending = node->particle.additiveBlending;
            
            device_.DrawParticlePass(pipelineHandle, drawParams, outputTexture, animatedUniforms);
            return;
        }
    }

    device_.DrawFullscreenPass(pipelineHandle, inputTextures, outputTexture, animatedUniforms);
    lastVideoFrameByNode_[pass.nodeId] = outputTexture;
}

Result<TextureHandle> TransientTexturePool::Acquire(const TextureDesc& desc) {
    for (auto& entry : pool_) {
        if (entry.idle && entry.desc.width == desc.width && entry.desc.height == desc.height &&
            entry.desc.format == desc.format && entry.desc.usage == desc.usage) {
            entry.idle = false;
            return Result<TextureHandle>::Ok(entry.handle);
        }
    }
    auto created = device_.CreateTexture(desc);
    if (!created) return created;
    pool_.push_back(Entry{created.value, desc, false});
    return created;
}

void TransientTexturePool::Release(TextureHandle handle) {
    for (auto& entry : pool_) {
        if (entry.handle == handle) { entry.idle = true; return; }
    }
}

void TransientTexturePool::EndFrame() {
    for (auto& entry : pool_) entry.idle = true;
}

// Particle system initialization
void RenderGraph::InitializeParticleSystem(const ParticleConfig& config) {
    if (particleState_.initialized) return;
    
    particleState_.maxParticles = config.maxParticles;
    
    // Create particle storage buffer (storage buffer for compute shader read/write)
    BufferDesc particleBufferDesc;
    particleBufferDesc.size = sizeof(vfx::Particle) * particleState_.maxParticles;
    particleBufferDesc.usage = BufferUsage::StorageBuffer;
    particleBufferDesc.hostVisible = false;
    particleBufferDesc.debugName = "particle_buffer";
    
    auto particleBufferResult = device_.CreateBuffer(particleBufferDesc);
    if (!particleBufferResult) {
        LOGE("Failed to create particle storage buffer");
        return;
    }
    particleState_.particleBuffer = particleBufferResult.value;
    
    // Create simulation params uniform buffer
    BufferDesc simParamsDesc;
    simParamsDesc.size = 256; // enough for sim params
    simParamsDesc.usage = BufferUsage::UniformBuffer;
    simParamsDesc.hostVisible = true;
    simParamsDesc.debugName = "particle_sim_params";
    
    auto simParamsResult = device_.CreateBuffer(simParamsDesc);
    if (!simParamsResult) {
        LOGE("Failed to create particle sim params buffer");
        return;
    }
    particleState_.simParamsBuffer = simParamsResult.value;
    
    // Create compute pipeline for particle simulation
    auto csHandle = GetOrCreateShaderModule(kParticleSimCompSpirv, kParticleSimCompSpirvWords);
    if (!csHandle.IsValid()) {
        LOGE("Failed to create particle compute shader module");
        return;
    }
    
    auto pipelineResult = device_.CreateComputePipeline(csHandle);
    if (!pipelineResult) {
        LOGE("Failed to create particle compute pipeline");
        return;
    }
    particleState_.computePipeline = pipelineResult.value;
    
    particleState_.initialized = true;
    particleState_.frameIndex = 0;
    particleState_.seed = config.seed ? config.seed : 12345;
    
    // Create indirect draw buffer (for alive particle count from compute shader)
    BufferDesc indirectBufferDesc;
    indirectBufferDesc.size = sizeof(VkDrawIndirectCommand);
    indirectBufferDesc.usage = BufferUsage::StorageBuffer | BufferUsage::IndirectBuffer;
    indirectBufferDesc.hostVisible = false;
    indirectBufferDesc.debugName = "particle_indirect_draw";
    
    auto indirectBufferResult = device_.CreateBuffer(indirectBufferDesc);
    if (indirectBufferResult) {
        particleState_.indirectBuffer = indirectBufferResult.value;
    }
}

void RenderGraph::DispatchParticleCompute(const CompiledPass& pass, double deltaTime, const ParticleConfig& config) {
    if (!particleState_.initialized || !config.useGpuParticles) return;
    
    // Pack host config into GPU sim params
    ParticleSimParams simParams = PackSimParams(config, deltaTime, particleState_.frameIndex, particleState_.seed);
    
    // Update sim params buffer
    VkBufferResource* simParamsBufRes = buffers_.Get(particleState_.simParamsBuffer);
    if (simParamsBufRes && simParamsBufRes->mapped) {
        std::memcpy(simParamsBufRes->mapped, &simParams, sizeof(ParticleSimParams));
    }
    
    // Dispatch compute shader
    uint32_t groupCount = (particleState_.maxParticles + 255) / 256; // 256 threads per workgroup
    device_.DispatchCompute(particleState_.computePipeline, groupCount, 1, 1);
    
    particleState_.frameIndex++;
}

ParticleSimParams RenderGraph::PackSimParams(const ParticleConfig& config, double deltaTime, uint32_t frameIndex, uint32_t seed) {
    ParticleSimParams params{};
    params.resolution[0] = 1920.0f; // TODO: get from swapchain
    params.resolution[1] = 1080.0f;
    params.deltaTime = static_cast<float>(deltaTime);
    params.emitRate = config.emitRate;
    params.emitRateVariation = config.emitRateVariation;
    params.emitterShape = static_cast<uint32_t>(config.emitterShape);
    params.emitPosition[0] = config.emitterPositionX;
    params.emitPosition[1] = config.emitterPositionY;
    params.emitRadius = config.emitRadius;
    params.emitLineStart[0] = config.emitLineStartX;
    params.emitLineStart[1] = config.emitLineStartY;
    params.emitLineEnd[0] = config.emitLineEndX;
    params.emitLineEnd[1] = config.emitLineEndY;
    params.initialLife = config.initialLife;
    params.lifeVariation = config.lifeVariation;
    params.initialSpeed = config.initialSpeed;
    params.speedVariation = config.speedVariation;
    params.emitAngle = config.emitAngle;
    params.angleVariation = config.angleVariation;
    params.gravity = config.gravity;
    params.windX = config.windX;
    params.windY = config.windY;
    params.turbulence = config.turbulence;
    params.drag = config.drag;
    params.deltaTimeInv = (deltaTime > 0.0) ? 1.0f / static_cast<float>(deltaTime) : 60.0f;
    params.maxParticles = particleState_.maxParticles;
    params.frameIndex = particleState_.frameIndex;
    params.seed = seed;

    return params;
}

void RenderGraph::RenderParticles(const CompiledPass& pass, const ParticleConfig& config, TextureHandle outputTexture,
                                  const std::vector<TextureHandle>& inputTextures, const std::unordered_map<std::string, float>& uniforms) {
    if (!particleState_.initialized) return;
    
    // Get the particle render pipeline
    // Bind particle buffer as storage buffer
    // Draw instanced (one instance per particle)
    // This would require a custom draw call in VulkanDevice
    
    // For now, fall back to regular draw
    // device_.DrawFullscreenPass(pipelineHandle, inputTextures, outputTexture, animatedUniforms);
}
