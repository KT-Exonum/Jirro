#pragma once
// Expression engine: QuickJS integration for per-property expressions
// Supports time, value, velocity, and custom variables

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>
#include <optional>
#include <mutex>

#include "quickjs/quickjs.h"

namespace vfx {

struct ExpressionContext {
    double time = 0.0;              // Current timeline time (seconds)
    double frameRate = 30.0;        // Project frame rate
    float value = 0.0f;             // Current property value
    float velocity = 0.0f;          // Current property velocity (value/sec)
    float prevValue = 0.0f;         // Previous frame value
    int frame = 0;                  // Current frame index
    
    // User-defined variables
    std::unordered_map<std::string, float> variables;
};

class ExpressionEngine {
public:
    ExpressionEngine();
    ~ExpressionEngine();
    
    // Compile an expression string to bytecode
    // Returns empty string on success, error message on failure
    std::string Compile(const std::string& expression);
    
    // Evaluate compiled expression with context
    // Returns std::nullopt on error
    std::optional<float> Evaluate(const std::string& expression, const ExpressionContext& ctx);
    
    // Evaluate with pre-compiled bytecode
    std::optional<float> EvaluateBytecode(const std::vector<uint8_t>& bytecode, const ExpressionContext& ctx);
    
    // Get bytecode for an expression (for caching)
    std::optional<std::vector<uint8_t>> GetBytecode(const std::string& expression);
    
    // Register a custom function
    void RegisterFunction(const std::string& name, std::function<float(const std::vector<float>&)> func);
    
    // Register a custom variable getter
    void RegisterVariable(const std::string& name, std::function<float(const ExpressionContext&)> getter);
    
    // Built-in functions available in expressions
    static void RegisterBuiltins(ExpressionEngine& engine);

private:
    struct CompiledExpression {
        std::vector<uint8_t> bytecode;
        std::string source;
        bool valid = false;
    };
    
    JSRuntime* rt_ = nullptr;
    JSContext* ctx_ = nullptr;
    
    std::unordered_map<std::string, CompiledExpression> cache_;
    std::mutex cacheMutex_;
    
    std::unordered_map<std::string, std::function<float(const std::vector<float>&)>> functions_;
    std::unordered_map<std::string, std::function<float(const ExpressionContext&)>> variables_;
    
    // JS evaluation helper
    std::optional<float> EvaluateJS(const std::string& code, const ExpressionContext& ctx);
    
    // Initialize JS context with builtins
    void InitContext();
    
    // Convert ExpressionContext to JS object
    JSValue ContextToJS(const ExpressionContext& ctx);
    
    // Register a JS function
    void RegisterJSFunction(const std::string& name, JSValue (*func)(JSContext*, JSValueConst, int, JSValueConst*));
};

// Standard expression functions
namespace ExprFuncs {
    // Math
    float clamp(float v, float min, float max);
    float lerp(float a, float b, float t);
    float smoothstep(float edge0, float edge1, float x);
    float smoothstep(float x);
    float fit(float v, float omin, float omax, float nmin, float nmax);
    float mix(float a, float b, float t);
    
    // Waveforms
    float sin(float x);
    float cos(float x);
    float tan(float x);
    float noise(float x);
    float noise2d(float x, float y);
    float fbm(float x, int octaves);
    float turbulence(float x, int octaves);
    
    // Time
    float time();                    // Current time
    float frame();                   // Current frame
    float framerate();               // Project frame rate
    float value();                   // Current value
    float velocity();                // Current velocity
    float prev();                    // Previous frame value
    float at(float t);               // Value at time t
    float atFrame(int f);            // Value at frame
    
    // Random
    float random(float min, float max);
    float gaussian(float mean, float stddev);
    float seed_random(int seed);
    
    // Color
    float rgb(float r, float g, float b);
    float hsv(float h, float s, float v);
    float lerpColor(float c1, float c2, float t);
    
    // Utility
    float abs(float x);
    float sign(float x);
    float floor(float x);
    float ceil(float x);
    float round(float x);
    float fract(float x);
    float mod(float x, float y);
    float pow(float x, float y);
    float sqrt(float x);
    float exp(float x);
    float log(float x);
    
    // Interpolation
    float linear(float t);
    float ease(float t);
    float easeIn(float t);
    float easeOut(float t);
    float easeInOut(float t);
    float bounce(float t);
    float elastic(float t);
    float back(float t);
    
    // Vector
    float length(float x, float y);
    float length3(float x, float y, float z);
    float dot(float x1, float y1, float x2, float y2);
    float cross(float x1, float y1, float x2, float y2);
    float angle(float x1, float y1, float x2, float y2);
}

} // namespace vfx