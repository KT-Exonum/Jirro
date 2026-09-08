#pragma once
// OCIO/ACES Color Management Pipeline
// Supports: sRGB, Rec.709, P3-D65, Rec.2020, ACEScc, ACEScct, ACES2065-1, LogC, SLog3, V-Log

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>
#include <array>
#include <string_view>

#include "engine/core/GraphicsDevice.h"

namespace vfx {

// Color spaces
enum class ColorSpace {
    // Display-referred
    sRGB,
    Rec709,
    P3_D65,
    Rec2020,
    
    // Scene-referred (linear)
    Linear_sRGB,
    Linear_Rec709,
    Linear_P3_D65,
    Linear_Rec2020,
    ACES2065_1,
    ACEScg,
    
    // Log encoding
    ACEScc,
    ACEScct,
    LogC_AWG,      // Arri LogC
    LogC_v3,       // Arri LogC v3
    SLog3,         // Sony SLog3
    SLog3_Cine,
    VLog,          // Panasonic V-Log
    CLog2,         // Canon C-Log 2
    CLog3,         // Canon C-Log 3
    RedLog3G10,    // Red Log3G10
    BRAW,          // Blackmagic RAW
    
    // HDR
    PQ,            // ST 2084 PQ
    HLG,           // Hybrid Log-Gamma
    
    // Raw
    Custom,
};

// Transfer functions
enum class TransferFunction {
    Linear,
    sRGB,
    Gamma22,
    Gamma24,
    Gamma26,
    PQ,
    HLG,
    LogC,
    SLog3,
    VLog,
    CLog2,
    CLog3,
    ACEScc,
    ACEScct,
    Custom,
};

// Primaries
enum class Primaries {
    sRGB,
    Rec709,
    P3_D65,
    Rec2020,
    ACES,
    Custom,
};

// White point
enum class WhitePoint {
    D65,
    D60,
    D55,
    ACES,       // ACES white point
    Custom,
};

// Color space descriptor
struct ColorSpaceDesc {
    ColorSpace space = ColorSpace::sRGB;
    TransferFunction transfer = TransferFunction::sRGB;
    Primaries primaries = Primaries::sRGB;
    WhitePoint whitePoint = WhitePoint::D65;
    
    // Custom parameters
    float customGamma = 2.2f;
    std::array<float, 9> customPrimaries = {0};
    std::array<float, 3> customWhitePoint = {0.3127f, 0.3290f, 1.0f}; // D65
    
    static ColorSpaceDesc FromColorSpace(ColorSpace cs);
    static ColorSpaceDesc FromString(std::string_view str);
    std::string ToString() const;
    
    bool operator==(const ColorSpaceDesc& other) const;
    bool operator!=(const ColorSpaceDesc& other) const { return !(*this == other); }
};

// OCIO config (simplified - would parse .ocio file in production)
struct OCIOConfig {
    std::string name = "ACES 1.3";
    std::vector<std::string> searchPaths;
    std::string display = "sRGB";
    std::string view = "ACES 1.0 - SDR Video";
    std::string look = "None";
    std::string direction = "forward"; // "forward" or "inverse"
    
    // LUT3D for 3D LUTs
    struct LUT3D {
        std::string name;
        int size = 33;
        std::vector<float> data; // size^3 * 3 floats (RGB)
    };
    std::vector<LUT3D> luts3d;
    
    // LUT1D for 1D LUTs (shaper)
    struct LUT1D {
        std::string name;
        int size = 1024;
        std::vector<float> data; // size * 3 floats (RGB)
    };
    std::vector<LUT1D> luts1d;
    
    // CDL (Color Decision List)
    struct CDL {
        std::string name;
        std::array<float, 3> slope = {1,1,1};
        std::array<float, 3> offset = {0,0,0};
        std::array<float, 3> power = {1,1,1};
        float saturation = 1.0f;
    };
    std::vector<CDL> cdls;
    
    // Matrix transforms
    struct MatrixTransform {
        std::string name;
        std::array<float, 16> matrix; // 4x4 row-major
        std::array<float, 4> offset = {0,0,0,0};
    };
    std::vector<MatrixTransform> matrices;
};

// Color processor - applies color transform
class ColorProcessor {
public:
    ColorProcessor() = default;
    explicit ColorProcessor(const OCIOConfig& config);
    
    // Set source/destination color spaces
    void SetColorSpaces(const ColorSpaceDesc& src, const ColorSpaceDesc& dst);
    
    // Apply transform to image (CPU)
    bool ProcessCPU(const float* src, float* dst, int width, int height, int channels = 4) const;
    
    // Get GPU shader for transform
    std::string GetGLSLTransform(const ColorSpaceDesc& src, const ColorSpaceDesc& dst) const;
    
    // Get uniform data for GPU
    struct GPUUniforms {
        std::array<float, 16> matrix;    // 4x4 matrix
        std::array<float, 4> offset;     // CDL offset
        std::array<float, 4> slope;      // CDL slope
        std::array<float, 4> power;      // CDL power
        float saturation = 1.0f;
        int lut3dIndex = -1;
        float lut3dScale = 1.0f;
        int lut3dSize = 33;
        int direction = 1; // 1 = forward, -1 = inverse
    };
    GPUUniforms GetGPUUniforms() const;
    
    // Create 3D LUT texture
    TextureHandle CreateLUT3DTexture(GraphicsDevice& device, int index) const;
    
    // Precompute LUT for fast GPU transform
    void PrecomputeLUT3D(int size = 33);
    
    // Check if transform is identity
    bool IsIdentity() const { return identity_; }

private:
    OCIOConfig config_;
    ColorSpaceDesc srcSpace_, dstSpace_;
    bool identity_ = false;
    
    // Transform pipeline
    struct TransformStep {
        enum Type { Matrix, LUT1D, LUT3D, CDL, Log, Gamma, Custom } type;
        std::array<float, 16> matrix;
        std::array<float, 4> offset;
        std::array<float, 4> slope;
        std::array<float, 4> power;
        float saturation = 1.0f;
        int lut1dIndex = -1;
        int lut3dIndex = -1;
        bool forward = true;
    };
    std::vector<TransformStep> pipeline_;
    
    // Build pipeline from color spaces
    void BuildPipeline();
    
    // Apply single step
    void ApplyStep(const TransformStep& step, float& r, float& g, float& b) const;
    
    // Transfer function encode/decode
    float EncodeTransfer(TransferFunction tf, float linear) const;
    float DecodeTransfer(TransferFunction tf, float encoded) const;
    
    // Primaries transform
    std::array<float, 9> GetPrimariesMatrix(Primaries from, Primaries to) const;
    
    // White point adaptation (Bradford)
    std::array<float, 9> GetChromaticAdaptation(WhitePoint from, WhitePoint to) const;
    
    // CDL application
    void ApplyCDL(const OCIOConfig::CDL& cdl, float& r, float& g, float& b) const;
    
    // Log encoding/decoding
    float LogEncode(float x, const OCIOConfig::CDL& cdl) const;
    float LogDecode(float x, const OCIOConfig& cdl) const;
    
    // PQ (ST 2084)
    float PQEncode(float linear) const;
    float PQDecode(float pq) const;
    
    // HLG
    float HLGEncode(float linear) const;
    float HLGDecode(float hlg) const;
    
    // LogC
    float LogCEncode(float linear, bool v3 = false) const;
    float LogCDecode(float logc, bool v3 = false) const;
};

// Global color manager
class ColorManager {
public:
    static ColorManager& Instance();
    
    // Load OCIO config from file
    bool LoadConfig(const std::string& path);
    bool LoadConfigFromString(const std::string& configString);
    
    // Get processor for transform
    std::shared_ptr<ColorProcessor> GetProcessor(const ColorSpaceDesc& src, const ColorSpaceDesc& dst);
    
    // Get current working space
    ColorSpaceDesc GetWorkingSpace() const { return workingSpace_; }
    void SetWorkingSpace(const ColorSpaceDesc& space) { workingSpace_ = space; }
    
    // Get display/view
    void SetDisplayView(const std::string& display, const std::string& view);
    std::string GetDisplay() const { return currentDisplay_; }
    std::string GetView() const { return currentView_; }
    
    // Get available displays/views
    std::vector<std::string> GetDisplays() const;
    std::vector<std::string> GetViews(const std::string& display) const;
    
    // Get LUT3D texture for current display
    TextureHandle GetDisplayLUT3D(GraphicsDevice& device);
    
    // Auto-detect color space from file metadata
    static ColorSpaceDesc DetectColorSpace(const std::string& filePath);
    
private:
    ColorManager() = default;
    OCIOConfig config_;
    ColorSpaceDesc workingSpace_ = ColorSpaceDesc::FromColorSpace(ColorSpace::ACEScg);
    std::string currentDisplay_ = "sRGB";
    std::string currentView_ = "ACES 1.0 - SDR Video";
    std::unordered_map<std::string, std::shared_ptr<ColorProcessor>> processorCache_;
    TextureHandle displayLUT3D_;
};

// ACES workflow helpers
namespace ACES {
    // Standard ACES transforms
    extern const ColorSpaceDesc ACES2065_1;
    extern const ColorSpaceDesc ACEScg;
    extern const ColorSpaceDesc ACEScct;
    extern const ColorSpaceDesc ACEScct_Linear;
    extern const ColorSpaceDesc ACEScc;
    
    // Output transforms
    extern const ColorSpaceDesc sRGB_Display;
    extern const ColorSpaceDesc P3_D65_Display;
    extern const ColorSpaceDesc Rec709_Display;
    extern const ColorSpaceDesc Rec2020_PQ_Display;
    extern const ColorSpaceDesc Rec2020_HLG_Display;
    
    // Camera log to ACES
    extern const ColorSpaceDesc ArriLogC3_to_ACES;
    extern const ColorSpaceDesc SonySLog3_to_ACES;
    extern const ColorSpaceDesc PanasonicVLog_to_ACES;
    extern const ColorSpaceDesc CanonCLog2_to_ACES;
    extern const ColorSpaceDesc CanonCLog3_to_ACES;
    extern const ColorSpaceDesc RedLog3G10_to_ACES;
    extern const ColorSpaceDesc BlackmagicBRAW_to_ACES;
    
    // Build standard ACES pipeline
    // Input -> IDT -> ACEScg -> RRT -> ODT -> Display
    std::shared_ptr<ColorProcessor> BuildACESPipeline(
        ColorSpaceDesc inputSpace,  // Camera color space
        ColorSpaceDesc outputSpace  // Display color space
    );
    
    // Quick transform
    void Transform(const float* src, float* dst, int width, int height,
                   ColorSpaceDesc srcSpace, ColorSpaceDesc dstSpace);
}

} // namespace vfx