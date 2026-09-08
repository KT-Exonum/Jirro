#ifdef ENGINE_ENABLE_EXPRESSION_ENGINE
#include "ExpressionEngine.h"

#include <quickjs/quickjs.h>
#include <android/log.h>
#include <cmath>
#include <random>

#define LOG_TAG "ExpressionEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vfx {

// ---------- Built-in function implementations ----------

namespace ExprFuncs {
    float clamp(float v, float min, float max) { return std::max(min, std::min(max, v)); }
    float lerp(float a, float b, float t) { return a + (b - a) * t; }
    float smoothstep(float edge0, float edge1, float x) {
        float t = std::max(0.0f, std::min(1.0f, (x - edge0) / (edge1 - edge0)));
        return t * t * (3.0f - 2.0f * t);
    }
    float smoothstep(float x) { return x * x * (3.0f - 2.0f * x); }
    float fit(float v, float omin, float omax, float nmin, float nmax) {
        return nmin + (v - omin) * (nmax - nmin) / (omax - omin);
    }
    float mix(float a, float b, float t) { return lerp(a, b, t); }
    
    float sin(float x) { return std::sin(x); }
    float cos(float x) { return std::cos(x); }
    float tan(float x) { return std::tan(x); }
    
    static std::mt19937 rng(std::random_device{}());
    float noise(float x) {
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        return dist(rng);
    }
    float noise2d(float x, float y) {
        // Simple 2D hash-based noise
        uint64_t h = (static_cast<uint64_t>(std::abs(x * 1000)) << 32) ^ static_cast<uint64_t>(std::abs(y * 1000));
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccd;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53;
        h ^= h >> 33;
        return static_cast<float>(h) / UINT64_MAX * 2.0f - 1.0f;
    }
    float fbm(float x, int octaves) {
        float value = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        for (int i = 0; i < octaves; ++i) {
            value += amplitude * noise(x * frequency);
            frequency *= 2.0f;
            amplitude *= 0.5f;
        }
        return value;
    }
    float turbulence(float x, int octaves) {
        float value = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        for (int i = 0; i < octaves; ++i) {
            value += amplitude * std::abs(noise(x * frequency));
            frequency *= 2.0f;
            amplitude *= 0.5f;
        }
        return value;
    }
    
    float time() { return 0.0f; } // Will be overridden by context
    float frame() { return 0.0f; }
    float framerate() { return 30.0f; }
    float value() { return 0.0f; }
    float velocity() { return 0.0f; }
    float prev() { return 0.0f; }
    float at(float t) { return 0.0f; }
    float atFrame(int f) { return 0.0f; }
    
    static std::mt19937 rng2(std::random_device{}());
    float random(float min, float max) {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(rng2);
    }
    float gaussian(float mean, float stddev) {
        std::normal_distribution<float> dist(mean, stddev);
        return dist(rng2);
    }
    float seed_random(int seed) {
        rng2.seed(seed);
        return 0.0f;
    }
    
    float rgb(float r, float g, float b) { return r * 65536 + g * 256 + b; }
    float hsv(float h, float s, float v) {
        int i = static_cast<int>(h * 6);
        float f = h * 6 - i;
        float p = v * (1 - s);
        float q = v * (1 - f * s);
        float t = v * (1 - (1 - f) * s);
        switch (i % 6) {
            case 0: return v * 65536 + t * 256 + p;
            case 1: return q * 65536 + v * 256 + p;
            case 2: return p * 65536 + v * 256 + t;
            case 3: return p * 65536 + q * 256 + v;
            case 4: return t * 65536 + p * 256 + v;
            case 5: return v * 65536 + p * 256 + q;
        }
        return 0;
    }
    float lerpColor(float c1, float c2, float t) { return c1 + (c2 - c1) * t; }
    
    float abs(float x) { return std::abs(x); }
    float sign(float x) { return (x > 0) - (x < 0); }
    float floor(float x) { return std::floor(x); }
    float ceil(float x) { return std::ceil(x); }
    float round(float x) { return std::round(x); }
    float fract(float x) { return x - std::floor(x); }
    float mod(float x, float y) { return std::fmod(x, y); }
    float pow(float x, float y) { return std::pow(x, y); }
    float sqrt(float x) { return std::sqrt(x); }
    float exp(float x) { return std::exp(x); }
    float log(float x) { return std::log(x); }
    
    float linear(float t) { return t; }
    float ease(float t) { return t * t * (3 - 2 * t); }
    float easeIn(float t) { return t * t; }
    float easeOut(float t) { return 1 - (1 - t) * (1 - t); }
    float easeInOut(float t) { return t < 0.5 ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t); }
    float bounce(float t) {
        if (t < 1/2.75f) return 7.5625f * t * t;
        if (t < 2/2.75f) return 7.5625f * (t - 1.5f/2.75f) * (t - 1.5f/2.75f) + 0.75f;
        if (t < 2.5f/2.75f) return 7.5625f * (t - 2.25f/2.75f) * (t - 2.25f/2.75f) + 0.9375f;
        return 7.5625f * (t - 2.625f/2.75f) * (t - 2.625f/2.75f) + 0.984375f;
    }
    float elastic(float t) {
        if (t == 0 || t == 1) return t;
        return -std::pow(2, 10 * (t - 1)) * std::sin((t - 1.1f) * 5 * std::numbers::pi_v<float>);
    }
    float back(float t) {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1;
        return c3 * t * t * t - c1 * t * t;
    }
    
    float length(float x, float y) { return std::sqrt(x*x + y*y); }
    float length3(float x, float y, float z) { return std::sqrt(x*x + y*y + z*z); }
    float dot(float x1, float y1, float x2, float y2) { return x1*x2 + y1*y2; }
    float cross(float x1, float y1, float x2, float y2) { return x1*y2 - y1*x2; }
    float angle(float x1, float y1, float x2, float y2) { return std::atan2(y2, x2) - std::atan2(y1, x1); }
}

// ---------- ExpressionEngine ----------

ExpressionEngine::ExpressionEngine() {
    rt_ = JS_NewRuntime();
    if (!rt_) {
        LOGE("Failed to create JS runtime");
        return;
    }
    ctx_ = JS_NewContext(rt_);
    if (!ctx_) {
        LOGE("Failed to create JS context");
        return;
    }
    InitContext();
}

ExpressionEngine::~ExpressionEngine() {
    if (ctx_) JS_FreeContext(ctx_);
    if (rt_) JS_FreeRuntime(rt_);
}

void ExpressionEngine::InitContext() {
    // Create global object with built-in functions
    JSValue global = JS_GetGlobalObject(ctx_);
    
    // Math object
    JSValue math = JS_NewObject(ctx_);
    JS_SetPropertyStr(ctx_, math, "PI", JS_NewFloat64(ctx_, std::numbers::pi_v<double>));
    JS_SetPropertyStr(ctx_, math, "E", JS_NewFloat64(ctx_, std::numbers::e_v<double>));
    JS_SetPropertyStr(ctx_, math, "sin", JS_NewCFunction(ctx_, [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
        if (argc < 1) return JS_EXCEPTION;
        double v; JS_ToFloat64(ctx, &v, argv[0]);
        return JS_NewFloat64(ctx, std::sin(v));
    }, "sin", 1));
    JS_SetPropertyStr(ctx_, math, "cos", JS_NewCFunction(ctx_, [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
        if (argc < 1) return JS_EXCEPTION;
        double v; JS_ToFloat64(ctx, &v, argv[0]);
        return JS_NewFloat64(ctx, std::cos(v));
    }, "cos", 1));
    JS_SetPropertyStr(ctx_, math, "tan", JS_NewCFunction(ctx_, [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
        if (argc < 1) return JS_EXCEPTION;
        double v; JS_ToFloat64(ctx, &v, argv[0]);
        return JS_NewFloat64(ctx, std::tan(v));
    }, "tan", 1));
    JS_SetPropertyStr(ctx_, math, "sqrt", JS_NewCFunction(ctx_, [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
        if (argc < 1) return JS_EXCEPTION;
        double v; JS_ToFloat64(ctx, &v, argv[0]);
        return JS_NewFloat64(ctx, std::sqrt(v));
    }, "sqrt", 1));
    JS_SetPropertyStr(ctx_, math, "pow", JS_NewCFunction(ctx_, [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
        if (argc < 2) return JS_EXCEPTION;
        double a, b; JS_ToFloat64(ctx, &a, argv[0]); JS_ToFloat64(ctx, &b, argv[1]);
        return JS_NewFloat64(ctx, std::pow(a, b));
    }, "pow", 2));
    JS_SetPropertyStr(ctx_, math, "abs", JS_NewCFunction(ctx_, [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
        if (argc < 1) return JS_EXCEPTION;
        double v; JS_ToFloat64(ctx, &v, argv[0]);
        return JS_NewFloat64(ctx, std::abs(v));
    }, "abs", 1));
    JS_SetPropertyStr(ctx_, math, "floor", JS_NewCFunction(ctx_, [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
        if (argc < 1) return JS_EXCEPTION;
        double v; JS_ToFloat64(ctx, &v, argv[0]);
        return JS_NewFloat64(ctx, std::floor(v));
    }, "floor", 1));
    JS_SetPropertyStr(ctx_, math, "ceil", JS_NewCFunction(ctx_, [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
        if (argc < 1) return JS_EXCEPTION;
        double v; JS_ToFloat64(ctx, &v, argv[0]);
        return JS_NewFloat64(ctx, std::ceil(v));
    }, "ceil", 1));
    JS_SetPropertyStr(ctx_, math, "random", JS_NewCFunction(ctx_, [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
        static std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return JS_NewFloat64(ctx, dist(rng));
    }, "random", 0));
    JS_SetPropertyStr(ctx_, global, "Math", math);
    
    // Global functions
    auto makeFunc = [&](const char* name, auto func) {
        JS_SetPropertyStr(ctx_, global, name, 
            JS_NewCFunction(ctx_, [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
                if (argc < 1) return JS_EXCEPTION;
                double v; JS_ToFloat64(ctx, &v, argv[0]);
                return JS_NewFloat64(ctx, func(v));
            }, name, 1));
    };
    
    makeFunc("sin", std::sin);
    makeFunc("cos", std::cos);
    makeFunc("tan", std::tan);
    makeFunc("sqrt", std::sqrt);
    makeFunc("abs", std::abs);
    makeFunc("floor", std::floor);
    makeFunc("ceil", std::ceil);
    makeFunc("round", [](double x) { return std::round(x); });
    makeFunc("pow", [](double x) { return std::pow(x, 2.0); }); // pow(x,2)
    makeFunc("sqrt", std::sqrt);
    makeFunc("exp", std::exp);
    makeFunc("log", std::log);
    
    JS_FreeValue(ctx_, global);
}

std::string ExpressionEngine::Compile(const std::string& expression) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    auto it = cache_.find(expression);
    if (it != cache_.end() && it->second.valid) {
        return ""; // Already compiled
    }
    
    // Compile to bytecode
    JSValue result = JS_Eval(ctx_, expression.c_str(), expression.size(), "<expression>", 
                             JS_EVAL_FLAG_COMPILE_ONLY);
    
    if (JS_IsException(result)) {
        JSValue err = JS_GetException(ctx_);
        const char* errStr = JS_ToCString(ctx_, err);
        std::string error = errStr ? errStr : "Unknown error";
        JS_FreeCString(ctx_, errStr);
        JS_FreeValue(ctx_, err);
        JS_FreeValue(ctx_, result);
        return error;
    }
    
    // Get bytecode
    size_t bytecodeLen;
    uint8_t* bytecode = JS_WriteObject(ctx_, &bytecodeLen, result, JS_WRITE_OBJ_BYTECODE);
    
    if (!bytecode) {
        JS_FreeValue(ctx_, result);
        return "Failed to generate bytecode";
    }
    
    CompiledExpression compiled;
    compiled.bytecode.assign(bytecode, bytecode + bytecodeLen);
    compiled.source = expression;
    compiled.valid = true;
    cache_[expression] = std::move(compiled);
    
    js_free(ctx_, bytecode);
    JS_FreeValue(ctx_, result);
    
    return "";
}

std::optional<float> ExpressionEngine::Evaluate(const std::string& expression, const ExpressionContext& ctx) {
    // Check cache
    std::vector<uint8_t> bytecode;
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = cache_.find(expression);
        if (it != cache_.end() && it->second.valid) {
            bytecode = it->second.bytecode;
        } else {
            // Compile first
            std::string err = Compile(expression);
            if (!err.empty()) {
                LOGE("Compile error: %s", err.c_str());
                return std::nullopt;
            }
            bytecode = cache_[expression].bytecode;
        }
    }
    
    return EvaluateBytecode(bytecode, ctx);
}

std::optional<float> ExpressionEngine::EvaluateBytecode(const std::vector<uint8_t>& bytecode, const ExpressionContext& ctx) {
    if (bytecode.empty()) return std::nullopt;
    
    // Read bytecode back to function
    JSValue func = JS_ReadObject(ctx_, bytecode.data(), bytecode.size(), JS_READ_OBJ_BYTECODE);
    if (JS_IsException(func)) return std::nullopt;
    
    // Create context object
    JSValue ctxObj = JS_NewObject(ctx_);
    JS_SetPropertyStr(ctx_, ctxObj, "time", JS_NewFloat64(ctx_, ctx.time));
    JS_SetPropertyStr(ctx_, ctxObj, "frameRate", JS_NewFloat64(ctx_, ctx.frameRate));
    JS_SetPropertyStr(ctx_, ctxObj, "value", JS_NewFloat64(ctx_, ctx.value));
    JS_SetPropertyStr(ctx_, ctxObj, "velocity", JS_NewFloat64(ctx_, ctx.velocity));
    JS_SetPropertyStr(ctx_, ctxObj, "prevValue", JS_NewFloat64(ctx_, ctx.prevValue));
    JS_SetPropertyStr(ctx_, ctxObj, "frame", JS_NewInt32(ctx_, ctx.frame));
    
    // Add user variables
    for (const auto& [name, val] : ctx.variables) {
        JS_SetPropertyStr(ctx_, ctxObj, name.c_str(), JS_NewFloat64(ctx_, val));
    }
    
    // Set as global 'ctx'
    JSValue global = JS_GetGlobalObject(ctx_);
    JS_SetPropertyStr(ctx_, global, "ctx", ctxObj);
    JS_FreeValue(ctx_, global);
    
    // Call function
    JSValue result = JS_Call(ctx_, func, JS_UNDEFINED, 0, nullptr);
    
    float ret = 0.0f;
    if (!JS_IsException(result)) {
        double val;
        if (JS_ToFloat64(ctx_, &val, result) == 0) {
            ret = static_cast<float>(val);
        }
    }
    
    JS_FreeValue(ctx_, result);
    JS_FreeValue(ctx_, func);
    JS_FreeValue(ctx_, ctxObj);
    
    return ret;
}

std::optional<std::vector<uint8_t>> ExpressionEngine::GetBytecode(const std::string& expression) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    auto it = cache_.find(expression);
    if (it != cache_.end() && it->second.valid) {
        return it->second.bytecode;
    }
    return std::nullopt;
}

void ExpressionEngine::RegisterFunction(const std::string& name, std::function<float(const std::vector<float>&)> func) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    functions_[name] = std::move(func);
    
    // Also register in JS context
    JSValue global = JS_GetGlobalObject(ctx_);
    JS_SetPropertyStr(ctx_, global, name.c_str(), 
        JS_NewCFunction(ctx_, [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            // Would need to look up the function and call it
            return JS_UNDEFINED;
        }, name.c_str(), 1));
    JS_FreeValue(ctx_, global);
}

void ExpressionEngine::RegisterVariable(const std::string& name, std::function<float(const ExpressionContext&)> getter) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    variables_[name] = std::move(getter);
}

void ExpressionEngine::RegisterBuiltins(ExpressionEngine& engine) {
    // Already done in InitContext
}

void ExpressionEngine::RegisterJSFunction(const std::string& name, JSValue (*func)(JSContext*, JSValueConst, int, JSValueConst*)) {
    JSValue global = JS_GetGlobalObject(ctx_);
    JS_SetPropertyStr(ctx_, global, name.c_str(), JS_NewCFunction(ctx_, func, name.c_str(), 1));
    JS_FreeValue(ctx_, global);
}

} // namespace vfx

#endif // ENGINE_ENABLE_EXPRESSION_ENGINE