#include "ProjectSerializer.h"

#include <android/log.h>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <format>
#include <iomanip>
#include <sstream>

#define LOG_TAG "ProjectSerializer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vfx {

// --- JsonValue minimal implementation ---

struct ProjectSerializer::JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    
    bool boolVal = false;
    double numVal = 0.0;
    std::string strVal;
    std::vector<JsonValue> arrVal;
    std::unordered_map<std::string, JsonValue> objVal;
    
    JsonValue() = default;
    JsonValue(bool v) : type(Bool), boolVal(v) {}
    JsonValue(double v) : type(Number), numVal(v) {}
    JsonValue(int v) : type(Number), numVal(v) {}
    JsonValue(const std::string& v) : type(String), strVal(v) {}
    JsonValue(const char* v) : type(String), strVal(v) {}
    JsonValue(std::vector<JsonValue>&& v) : type(Array), arrVal(std::move(v)) {}
    JsonValue(std::unordered_map<std::string, JsonValue>&& v) : type(Object), objVal(std::move(v)) {}
    
    JsonValue& operator[](const std::string& key) { return objVal[key]; }
    const JsonValue& operator[](const std::string& key) const { return objVal.at(key); }
    
    void push_back(const JsonValue& v) { arrVal.push_back(v); }
    void push_back(JsonValue&& v) { arrVal.push_back(std::move(v)); }
};

// --- JsonValue serialization ---

void ProjectSerializer::WriteJsonValue(std::ostream& os, const JsonValue& value, int indent) {
    std::string ind(indent * 2, ' ');
    std::string indNext((indent + 1) * 2, ' ');
    
    switch (value.type) {
        case JsonValue::Null:
            os << "null";
            break;
        case JsonValue::Bool:
            os << (value.boolVal ? "true" : "false");
            break;
        case JsonValue::Number:
            os << value.numVal;
            break;
        case JsonValue::String: {
            os << '"';
            for (char c : value.strVal) {
                switch (c) {
                    case '"': os << "\\\""; break;
                    case '\\': os << "\\\\"; break;
                    case '\n': os << "\\n"; break;
                    case '\r': os << "\\r"; break;
                    case '\t': os << "\\t"; break;
                    default: os << c; break;
                }
            }
            os << '"';
            break;
        }
        case JsonValue::Array: {
            os << "[\n";
            for (size_t i = 0; i < value.arrVal.size(); ++i) {
                os << indNext;
                WriteJsonValue(os, value.arrVal[i], indent + 1);
                if (i + 1 < value.arrVal.size()) os << ",";
                os << "\n";
            }
            os << ind << "]";
            break;
        }
        case JsonValue::Object: {
            os << "{\n";
            bool first = true;
            for (const auto& [key, val] : value.objVal) {
                if (!first) os << ",\n";
                first = false;
                os << indNext << '"' << key << "\": ";
                WriteJsonValue(os, val, indent + 1);
            }
            os << "\n" << ind << "}";
            break;
        }
    }
}

bool ProjectSerializer::ReadJsonValue(std::istream& is, JsonValue& value) {
    // Kept for API compatibility; LoadFromFile uses JsonDeserialize instead
    return false;
}

bool ProjectSerializer::JsonDeserialize(const std::string& json, ProjectData& data) {
    // Minimal JSON parser for our specific project format
    auto getString = [](const std::string& json, size_t& pos, const std::string& key) -> std::string {
        size_t keyPos = json.find("\"" + key + "\"", pos);
        if (keyPos == std::string::npos) return "";
        keyPos = json.find(':', keyPos);
        if (keyPos == std::string::npos) return "";
        keyPos++;
        while (keyPos < json.size() && (json[keyPos] == ' ' || json[keyPos] == '\t' || json[keyPos] == '\n')) keyPos++;
        if (keyPos >= json.size() || json[keyPos] != '"') return "";
        keyPos++;
        size_t end = json.find('"', keyPos);
        if (end == std::string::npos) return "";
        pos = end + 1;
        return json.substr(keyPos, end - keyPos);
    };
    
    auto getNumber = [](const std::string& json, size_t& pos, const std::string& key) -> double {
        size_t keyPos = json.find("\"" + key + "\"", pos);
        if (keyPos == std::string::npos) return 0.0;
        keyPos = json.find(':', keyPos);
        if (keyPos == std::string::npos) return 0.0;
        keyPos++;
        while (keyPos < json.size() && (json[keyPos] == ' ' || json[keyPos] == '\t' || json[keyPos] == '\n')) keyPos++;
        size_t end = keyPos;
        while (end < json.size() && (isdigit(json[end]) || json[end] == '.' || json[end] == '-' || json[end] == 'e' || json[end] == 'E')) end++;
        std::string numStr = json.substr(keyPos, end - keyPos);
        pos = end;
        return std::stod(numStr);
    };
    
    auto getBool = [](const std::string& json, size_t& pos, const std::string& key) -> bool {
        size_t keyPos = json.find("\"" + key + "\"", pos);
        if (keyPos == std::string::npos) return false;
        keyPos = json.find(':', keyPos);
        if (keyPos == std::string::npos) return false;
        keyPos++;
        while (keyPos < json.size() && (json[keyPos] == ' ' || json[keyPos] == '\t')) keyPos++;
        pos = keyPos + (json.substr(keyPos, 4) == "true" ? 4 : 5);
        return json.substr(keyPos, 4) == "true";
    };
    
    // Parse metadata
    size_t pos = 0;
    data.metadata.version = static_cast<int>(getNumber(json, pos, "version"));
    data.metadata.name = getString(json, pos, "name");
    data.metadata.duration = getNumber(json, pos, "duration");
    data.metadata.frameRate = getNumber(json, pos, "frameRate");
    data.metadata.width = static_cast<uint32_t>(getNumber(json, pos, "width"));
    data.metadata.height = static_cast<uint32_t>(getNumber(json, pos, "height"));
    
    // Parse nodes
    size_t nodesStart = json.find("\"nodes\"");
    if (nodesStart != std::string::npos) {
        size_t arrStart = json.find('[', nodesStart);
        if (arrStart != std::string::npos) {
            size_t arrEnd = json.find(']', arrStart);
            std::string nodesJson = json.substr(arrStart, arrEnd - arrStart + 1);
            
            size_t nodePos = 0;
            while (true) {
                size_t nodeStart = nodesJson.find("{", nodePos);
                if (nodeStart == std::string::npos) break;
                size_t nodeEnd = nodesJson.find("}", nodeStart);
                if (nodeEnd == std::string::npos) break;
                
                std::string nodeJson = nodesJson.substr(nodeStart, nodeEnd - nodeStart + 1);
                SerializedNode sn;
                sn.type = getString(nodeJson, pos, "type");
                sn.id = getString(nodeJson, pos, "id");
                sn.name = getString(nodeJson, pos, "name");
                sn.x = static_cast<float>(getNumber(nodeJson, pos, "x"));
                sn.y = static_cast<float>(getNumber(nodeJson, pos, "y"));
                sn.sourceFilePath = getString(nodeJson, pos, "sourceFilePath");
                sn.blendMode = static_cast<int>(getNumber(nodeJson, pos, "blendMode"));
                
                // Audio config
                size_t audioPos = nodeJson.find("\"audio\"");
                if (audioPos != std::string::npos) {
                    size_t audioEnd = nodeJson.find('}', audioPos);
                    std::string audioJson = nodeJson.substr(audioPos, audioEnd - audioPos + 1);
                    size_t ap = 0;
                    sn.audioSourceClipId = getString(audioJson, ap, "sourceClipId");
                    sn.audioSensitivity = static_cast<float>(getNumber(audioJson, ap, "sensitivity"));
                    sn.audioSmoothing = static_cast<float>(getNumber(audioJson, ap, "smoothing"));
                    sn.audioFrequencyMin = static_cast<float>(getNumber(audioJson, ap, "frequencyMin"));
                    sn.audioFrequencyMax = static_cast<float>(getNumber(audioJson, ap, "frequencyMax"));
                    sn.audioFftSize = static_cast<int>(getNumber(audioJson, ap, "fftSize"));
                    sn.audioUseBeatDetection = getBool(audioJson, ap, "useBeatDetection");
                    sn.audioBeatThreshold = static_cast<float>(getNumber(audioJson, ap, "beatThreshold"));
                    sn.audioWaveformPoints = static_cast<int>(getNumber(audioJson, ap, "waveformPoints"));
                    sn.audioSpectrumBars = static_cast<int>(getNumber(audioJson, ap, "spectrumBars"));
                    sn.audioBarWidth = static_cast<float>(getNumber(audioJson, ap, "barWidth"));
                    sn.audioBarGap = static_cast<float>(getNumber(audioJson, ap, "barGap"));
                    sn.audioBarColor = static_cast<uint32_t>(getNumber(audioJson, ap, "barColor"));
                    sn.audioBackgroundColor = static_cast<uint32_t>(getNumber(audioJson, ap, "backgroundColor"));
                }
                
                data.nodes.push_back(std::move(sn));
                nodePos = nodeEnd + 1;
            }
        }
    }
    
    // Parse connections
    size_t connStart = json.find("\"connections\"");
    if (connStart != std::string::npos) {
        size_t arrStart = json.find('[', connStart);
        if (arrStart != std::string::npos) {
            size_t arrEnd = json.find(']', arrStart);
            std::string connJson = json.substr(arrStart, arrEnd - arrStart + 1);
            
            size_t connPos = 0;
            while (true) {
                size_t connStartPos = connJson.find("{", connPos);
                if (connStartPos == std::string::npos) break;
                size_t connEnd = connJson.find("}", connStartPos);
                if (connEnd == std::string::npos) break;
                
                std::string singleConn = connJson.substr(connStartPos, connEnd - connStartPos + 1);
                SerializedConnection sc;
                sc.fromNodeId = getString(singleConn, connPos, "fromNodeId");
                sc.fromPort = getString(singleConn, connPos, "fromPort");
                sc.toNodeId = getString(singleConn, connPos, "toNodeId");
                sc.toPort = getString(singleConn, connPos, "toPort");
                data.connections.push_back(std::move(sc));
                connPos = connEnd + 1;
            }
        }
    }
    
    // Parse clips
    size_t clipsStart = json.find("\"clips\"");
    if (clipsStart != std::string::npos) {
        size_t arrStart = json.find('[', clipsStart);
        if (arrStart != std::string::npos) {
            size_t arrEnd = json.find(']', arrStart);
            std::string clipsJson = json.substr(arrStart, arrEnd - arrStart + 1);
            
            size_t clipPos = 0;
            while (true) {
                size_t clipStart = clipsJson.find("{", clipPos);
                if (clipStart == std::string::npos) break;
                size_t clipEnd = clipsJson.find("}", clipStart);
                if (clipEnd == std::string::npos) break;
                
                std::string singleClip = clipsJson.substr(clipStart, clipEnd - clipStart + 1);
                SerializedClip sc;
                sc.id = getString(singleClip, clipPos, "id");
                sc.sourceNodeId = getString(singleClip, clipPos, "sourceNodeId");
                sc.timelineStart = getNumber(singleClip, clipPos, "timelineStart");
                sc.sourceInPoint = getNumber(singleClip, clipPos, "sourceInPoint");
                sc.sourceOutPoint = getNumber(singleClip, clipPos, "sourceOutPoint");
                sc.playbackSpeed = getNumber(singleClip, clipPos, "playbackSpeed");
                sc.layer = static_cast<int>(getNumber(singleClip, clipPos, "layer"));
                data.clips.push_back(std::move(sc));
                clipPos = clipEnd + 1;
            }
        }
    }
    
    return true;
}

// --- NodeKind/BlendMode/Interpolation conversions ---

std::string ProjectSerializer::NodeKindToString(NodeKind kind) {
    switch (kind) {
        case NodeKind::VideoSource: return "VideoSource";
        case NodeKind::ImageSource: return "ImageSource";
        case NodeKind::AudioSource: return "AudioSource";
        case NodeKind::Shader: return "Shader";
        case NodeKind::Blend: return "Blend";
        case NodeKind::ColorCorrection: return "ColorCorrection";
        case NodeKind::Blur: return "Blur";
        case NodeKind::Mask: return "Mask";
        case NodeKind::Composite: return "Composite";
        case NodeKind::Output: return "Output";
        case NodeKind::Adjustment: return "Adjustment";
        case NodeKind::Null: return "Null";
        case NodeKind::Group: return "Group";
        case NodeKind::VectorSource: return "VectorSource";
        case NodeKind::TextSource: return "TextSource";
        case NodeKind::StrokeSource: return "StrokeSource";
        case NodeKind::MotionBlur: return "MotionBlur";
        case NodeKind::DirectionalBlur: return "DirectionalBlur";
        case NodeKind::TransformBlur: return "TransformBlur";
        case NodeKind::VelocityGraph: return "VelocityGraph";
        case NodeKind::TimeRemap: return "TimeRemap";
        case NodeKind::OpticalFlow: return "OpticalFlow";
        case NodeKind::BezierMask: return "BezierMask";
        case NodeKind::Rotoscoping: return "Rotoscoping";
        case NodeKind::RotoBrush: return "RotoBrush";
        case NodeKind::Tracker: return "Tracker";
        case NodeKind::ParticleEmitter: return "ParticleEmitter";
        case NodeKind::ParticleForces: return "ParticleForces";
        case NodeKind::ParticleRenderer: return "ParticleRenderer";
        case NodeKind::ShapeRectangle: return "ShapeRectangle";
        case NodeKind::ShapeEllipse: return "ShapeEllipse";
        case NodeKind::ShapePolygon: return "ShapePolygon";
        case NodeKind::ShapeStar: return "ShapeStar";
        case NodeKind::ShapePath: return "ShapePath";
        case NodeKind::ShapeRender: return "ShapeRender";
        case NodeKind::ShapeMerge: return "ShapeMerge";
        case NodeKind::ShapeTransform: return "ShapeTransform";
        case NodeKind::ShapeStroke: return "ShapeStroke";
        case NodeKind::ShapeFill: return "ShapeFill";
        case NodeKind::ShapeRepeater: return "ShapeRepeater";
        case NodeKind::ShapeBoolean: return "ShapeBoolean";
        case NodeKind::Transform3D: return "Transform3D";
        case NodeKind::Camera3D: return "Camera3D";
        case NodeKind::DepthOfField: return "DepthOfField";
        case NodeKind::ChromaKey: return "ChromaKey";
        case NodeKind::MeshSource: return "MeshSource";
        case NodeKind::AudioReactive: return "AudioReactive";
        case NodeKind::AudioWaveform: return "AudioWaveform";
        case NodeKind::AudioSpectrum: return "AudioSpectrum";
        // Motion Effects - Transform Motion
        case NodeKind::Oscillate: return "Oscillate";
        case NodeKind::Shake: return "Shake";
        case NodeKind::RandomDisplacement: return "RandomDisplacement";
        case NodeKind::Pulse: return "Pulse";
        case NodeKind::Swing: return "Swing";
        case NodeKind::Bounce: return "Bounce";
        case NodeKind::Elastic: return "Elastic";
        // Motion Effects - Camera Motion
        case NodeKind::CameraShake: return "CameraShake";
        case NodeKind::ZoomBlur: return "ZoomBlur";
        case NodeKind::RadialBlur: return "RadialBlur";
        // Motion Effects - Distortion Motion
        case NodeKind::Ripple: return "Ripple";
        case NodeKind::Wave: return "Wave";
        case NodeKind::Twist: return "Twist";
        case NodeKind::Bulge: return "Bulge";
        case NodeKind::Vortex: return "Vortex";
        // Motion Effects - Stylize Motion
        case NodeKind::Glitch: return "Glitch";
        case NodeKind::VHS: return "VHS";
        case NodeKind::Scanlines: return "Scanlines";
        case NodeKind::CRT: return "CRT";
        case NodeKind::ChromaticAberration: return "ChromaticAberration";
        case NodeKind::RGBShift: return "RGBShift";
        // Motion Effects - Time Motion
        case NodeKind::TimeStretch: return "TimeStretch";
        case NodeKind::FrameBlend: return "FrameBlend";
        case NodeKind::StopMotion: return "StopMotion";
        case NodeKind::PosterizeTime: return "PosterizeTime";
        // Motion Effects - Utility Motion
        case NodeKind::Wiggle: return "Wiggle";
        case NodeKind::Jitter: return "Jitter";
        case NodeKind::Drift: return "Drift";
        case NodeKind::Orbit: return "Orbit";
        // Resolve FX inspired
        case NodeKind::CameraShakePro: return "CameraShakePro";
        case NodeKind::DynamicZoom: return "DynamicZoom";
        case NodeKind::FilmDamage: return "FilmDamage";
        case NodeKind::FilmGrain: return "FilmGrain";
        case NodeKind::Vignette: return "Vignette";
        case NodeKind::Letterbox: return "Letterbox";
        // Advanced
        case NodeKind::BezierWarp: return "BezierWarp";
        case NodeKind::MeshWarp: return "MeshWarp";
        case NodeKind::PolarCoordinates: return "PolarCoordinates";
        case NodeKind::DisplacementMap: return "DisplacementMap";
        default: return "Unknown";
    }
}

NodeKind ProjectSerializer::StringToNodeKind(const std::string& str) {
    if (str == "VideoSource") return NodeKind::VideoSource;
    if (str == "ImageSource") return NodeKind::ImageSource;
    if (str == "AudioSource") return NodeKind::AudioSource;
    if (str == "Shader") return NodeKind::Shader;
    if (str == "Blend") return NodeKind::Blend;
    if (str == "ColorCorrection") return NodeKind::ColorCorrection;
    if (str == "Blur") return NodeKind::Blur;
    if (str == "Mask") return NodeKind::Mask;
    if (str == "Composite") return NodeKind::Composite;
    if (str == "Output") return NodeKind::Output;
    if (str == "Adjustment") return NodeKind::Adjustment;
    if (str == "Null") return NodeKind::Null;
    if (str == "Group") return NodeKind::Group;
    if (str == "VectorSource") return NodeKind::VectorSource;
    if (str == "TextSource") return NodeKind::TextSource;
    if (str == "StrokeSource") return NodeKind::StrokeSource;
    if (str == "MotionBlur") return NodeKind::MotionBlur;
    if (str == "DirectionalBlur") return NodeKind::DirectionalBlur;
    if (str == "TransformBlur") return NodeKind::TransformBlur;
    if (str == "VelocityGraph") return NodeKind::VelocityGraph;
    if (str == "TimeRemap") return NodeKind::TimeRemap;
    if (str == "OpticalFlow") return NodeKind::OpticalFlow;
    if (str == "BezierMask") return NodeKind::BezierMask;
    if (str == "Rotoscoping") return NodeKind::Rotoscoping;
    if (str == "RotoBrush") return NodeKind::RotoBrush;
    if (str == "Tracker") return NodeKind::Tracker;
    if (str == "ParticleEmitter") return NodeKind::ParticleEmitter;
    if (str == "ParticleForces") return NodeKind::ParticleForces;
    if (str == "ParticleRenderer") return NodeKind::ParticleRenderer;
    if (str == "ShapeRectangle") return NodeKind::ShapeRectangle;
    if (str == "ShapeEllipse") return NodeKind::ShapeEllipse;
    if (str == "ShapePolygon") return NodeKind::ShapePolygon;
    if (str == "ShapeStar") return NodeKind::ShapeStar;
    if (str == "ShapePath") return NodeKind::ShapePath;
    if (str == "ShapeRender") return NodeKind::ShapeRender;
    if (str == "ShapeMerge") return NodeKind::ShapeMerge;
    if (str == "ShapeTransform") return NodeKind::ShapeTransform;
    if (str == "ShapeStroke") return NodeKind::ShapeTransform;
    if (str == "ShapeFill") return NodeKind::ShapeFill;
    if (str == "ShapeRepeater") return NodeKind::ShapeRepeater;
    if (str == "ShapeBoolean") return NodeKind::ShapeBoolean;
    if (str == "Transform3D") return NodeKind::Transform3D;
    if (str == "Camera3D") return NodeKind::Camera3D;
    if (str == "DepthOfField") return NodeKind::DepthOfField;
    if (str == "ChromaKey") return NodeKind::ChromaKey;
    if (str == "MeshSource") return NodeKind::MeshSource;
    if (str == "AudioReactive") return NodeKind::AudioReactive;
    if (str == "AudioWaveform") return NodeKind::AudioWaveform;
    if (str == "AudioSpectrum") return NodeKind::AudioSpectrum;
    // Motion Effects - Transform Motion
    if (str == "Oscillate") return NodeKind::Oscillate;
    if (str == "Shake") return NodeKind::Shake;
    if (str == "RandomDisplacement") return NodeKind::RandomDisplacement;
    if (str == "Pulse") return NodeKind::Pulse;
    if (str == "Swing") return NodeKind::Swing;
    if (str == "Bounce") return NodeKind::Bounce;
    if (str == "Elastic") return NodeKind::Elastic;
    // Motion Effects - Camera Motion
    if (str == "CameraShake") return NodeKind::CameraShake;
    if (str == "ZoomBlur") return NodeKind::ZoomBlur;
    if (str == "RadialBlur") return NodeKind::RadialBlur;
    if (str == "MotionBlur") return NodeKind::MotionBlur;
    if (str == "DirectionalBlur") return NodeKind::DirectionalBlur;
    // Motion Effects - Distortion Motion
    if (str == "Ripple") return NodeKind::Ripple;
    if (str == "Wave") return NodeKind::Wave;
    if (str == "Twist") return NodeKind::Twist;
    if (str == "Bulge") return NodeKind::Bulge;
    if (str == "Vortex") return NodeKind::Vortex;
    // Motion Effects - Stylize Motion
    if (str == "Glitch") return NodeKind::Glitch;
    if (str == "VHS") return NodeKind::VHS;
    if (str == "Scanlines") return NodeKind::Scanlines;
    if (str == "CRT") return NodeKind::CRT;
    if (str == "ChromaticAberration") return NodeKind::ChromaticAberration;
    if (str == "RGBShift") return NodeKind::RGBShift;
    // Motion Effects - Time Motion
    if (str == "TimeStretch") return NodeKind::TimeStretch;
    if (str == "FrameBlend") return NodeKind::FrameBlend;
    if (str == "StopMotion") return NodeKind::StopMotion;
    if (str == "PosterizeTime") return NodeKind::PosterizeTime;
    // Motion Effects - Utility Motion
    if (str == "Wiggle") return NodeKind::Wiggle;
    if (str == "Jitter") return NodeKind::Jitter;
    if (str == "Drift") return NodeKind::Drift;
    if (str == "Orbit") return NodeKind::Orbit;
    // Resolve FX inspired
    if (str == "CameraShakePro") return NodeKind::CameraShakePro;
    if (str == "DynamicZoom") return NodeKind::DynamicZoom;
    if (str == "FilmDamage") return NodeKind::FilmDamage;
    if (str == "FilmGrain") return NodeKind::FilmGrain;
    if (str == "Vignette") return NodeKind::Vignette;
    if (str == "Letterbox") return NodeKind::Letterbox;
    // Advanced
    if (str == "BezierWarp") return NodeKind::BezierWarp;
    if (str == "MeshWarp") return NodeKind::MeshWarp;
    if (str == "PolarCoordinates") return NodeKind::PolarCoordinates;
    if (str == "DisplacementMap") return NodeKind::DisplacementMap;
    return NodeKind::Shader;
}

std::string ProjectSerializer::BlendModeToString(BlendMode mode) {
    switch (mode) {
        case BlendMode::Normal: return "Normal";
        case BlendMode::Multiply: return "Multiply";
        case BlendMode::Screen: return "Screen";
        case BlendMode::Overlay: return "Overlay";
        case BlendMode::Add: return "Add";
        case BlendMode::Subtract: return "Subtract";
        default: return "Normal";
    }
}

BlendMode ProjectSerializer::StringToBlendMode(const std::string& str) {
    if (str == "Normal") return BlendMode::Normal;
    if (str == "Multiply") return BlendMode::Multiply;
    if (str == "Screen") return BlendMode::Screen;
    if (str == "Overlay") return BlendMode::Overlay;
    if (str == "Add") return BlendMode::Add;
    if (str == "Subtract") return BlendMode::Subtract;
    return BlendMode::Normal;
}

std::string ProjectSerializer::InterpolationTypeToString(int type) {
    switch (type) {
        case 0: return "Step";
        case 1: return "Linear";
        case 2: return "Bezier";
        case 3: return "Custom";
        default: return "Linear";
    }
}

int ProjectSerializer::StringToInterpolationType(const std::string& str) {
    if (str == "Step") return 0;
    if (str == "Linear") return 1;
    if (str == "Bezier") return 2;
    if (str == "Custom") return 3;
    return 1;
}

// --- Get current ISO8601 timestamp ---
std::string ProjectSerializer::GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&time, &tm);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

// --- Serialize ---

ProjectData ProjectSerializer::Serialize(const NodeGraph& graph, const Timeline& timeline) {
    ProjectData data;
    
    // Metadata
    data.metadata.createdDate = GetCurrentTimestamp();
    data.metadata.modifiedDate = GetCurrentTimestamp();
    data.metadata.duration = 10.0; // Would get from timeline
    data.metadata.frameRate = timeline.FrameRate();
    
    // Nodes
    for (const auto& [id, node] : graph.AllNodes()) {
        SerializedNode sn;
        sn.id = node.nodeId;
        sn.type = NodeKindToString(node.kind);
        sn.name = node.debugName;
        
        // Ports
        for (const auto& port : node.inputs) sn.inputPorts.push_back(port.slotName);
        for (const auto& port : node.outputs) sn.outputPorts.push_back(port.slotName);
        
        // Uniforms
        sn.uniforms = node.uniformFloats;
        
        // Animated uniforms
        for (const auto& [name, track] : node.animatedUniforms) {
            SerializedNode::AnimatedUniform au;
            au.name = name;
            for (const auto& kf : track.Keyframes()) {
                SerializedNode::AnimatedUniform::Keyframe kfData;
                kfData.time = kf.timestamp;
                kfData.value = kf.value;
                kfData.interpolation = static_cast<int>(kf.interpolation);
                kfData.inTangent = kf.nextInHandle.valueOffset;
                kfData.outTangent = kf.outHandle.valueOffset;
                au.keyframes.push_back(kfData);
            }
            sn.animatedUniforms.push_back(std::move(au));
        }
        
        // Node-specific
        sn.sourceFilePath = node.sourceFilePath;
        sn.blendMode = static_cast<int>(node.blendMode);
        sn.spirvFragment = node.spirvFragment;
        if (node.kind == NodeKind::AudioReactive || node.kind == NodeKind::AudioWaveform || node.kind == NodeKind::AudioSpectrum) {
            sn.audioSourceClipId = node.audio.sourceClipId;
            sn.audioSensitivity = node.audio.sensitivity;
            sn.audioSmoothing = node.audio.smoothing;
            sn.audioFrequencyMin = node.audio.frequencyMin;
            sn.audioFrequencyMax = node.audio.frequencyMax;
            sn.audioFftSize = node.audio.fftSize;
            sn.audioUseBeatDetection = node.audio.useBeatDetection;
            sn.audioBeatThreshold = node.audio.beatThreshold;
            sn.audioWaveformPoints = node.audio.waveformPoints;
            sn.audioSpectrumBars = node.audio.spectrumBars;
            sn.audioBarWidth = node.audio.barWidth;
            sn.audioBarGap = node.audio.barGap;
            sn.audioBarColor = node.audio.barColor;
            sn.audioBackgroundColor = node.audio.backgroundColor;
        }
        
        data.nodes.push_back(std::move(sn));
    }
    
    // Connections
    for (const auto& conn : graph.AllConnections()) {
        SerializedConnection sc;
        sc.fromNodeId = conn.fromNodeId;
        sc.fromPort = conn.fromSlot;
        sc.toNodeId = conn.toNodeId;
        sc.toPort = conn.toSlot;
        data.connections.push_back(std::move(sc));
    }
    
    // Clips
    for (const auto& clip : timeline.AllClips()) {
        SerializedClip sc;
        sc.id = clip.clipId;
        sc.sourceNodeId = clip.sourceNodeId;
        sc.timelineStart = clip.timelineStart;
        sc.sourceInPoint = clip.sourceInPoint;
        sc.sourceOutPoint = clip.sourceOutPoint;
        sc.playbackSpeed = clip.playbackSpeed;
        sc.layer = clip.layer;
        data.clips.push_back(std::move(sc));
    }
    
    // Transitions
    for (const auto& trans : timeline.AllTransitions()) {
        SerializedTransition st;
        st.fromClipId = trans.fromClipId;
        st.toClipId = trans.toClipId;
        st.duration = trans.duration;
        st.blendShaderNodeId = trans.blendShaderNodeId;
        data.transitions.push_back(std::move(st));
    }
    
    return data;
}

// --- Deserialize ---

bool ProjectSerializer::Deserialize(const ProjectData& data, NodeGraph& graph, Timeline& timeline) {
    try {
        // Clear existing
        graph.Clear();
        timeline = Timeline(data.metadata.frameRate);
        
        // Nodes
        for (const auto& sn : data.nodes) {
            Node node;
            node.nodeId = sn.id;
            node.kind = StringToNodeKind(sn.type);
            node.debugName = sn.name;
            
            // Ports
            for (const auto& portName : sn.inputPorts) {
                node.inputs.push_back({portName});
            }
            for (const auto& portName : sn.outputPorts) {
                node.outputs.push_back({portName});
            }
            
            // Uniforms
            node.uniformFloats = sn.uniforms;
            
            // Animated uniforms
            for (const auto& au : sn.animatedUniforms) {
                KeyframeTrack track;
                for (const auto& kf : au.keyframes) {
                    Keyframe kfData;
                    kfData.timestamp = kf.time;
                    kfData.value = kf.value;
                    kfData.interpolation = static_cast<InterpolationType>(kf.interpolation);
                    kfData.outHandle.valueOffset = kf.outTangent;
                    kfData.nextInHandle.valueOffset = kf.inTangent;
                    track.AddKeyframe(kfData);
                }
                node.animatedUniforms[au.name] = std::move(track);
            }
            
            // Node-specific
            node.sourceFilePath = sn.sourceFilePath;
            node.blendMode = static_cast<BlendMode>(sn.blendMode);
            node.spirvFragment = sn.spirvFragment;
            if (node.kind == NodeKind::AudioReactive || node.kind == NodeKind::AudioWaveform || node.kind == NodeKind::AudioSpectrum) {
                node.audio.sourceClipId = sn.audioSourceClipId;
                node.audio.sensitivity = sn.audioSensitivity;
                node.audio.smoothing = sn.audioSmoothing;
                node.audio.frequencyMin = sn.audioFrequencyMin;
                node.audio.frequencyMax = sn.audioFrequencyMax;
                node.audio.fftSize = sn.audioFftSize;
                node.audio.useBeatDetection = sn.audioUseBeatDetection;
                node.audio.beatThreshold = sn.audioBeatThreshold;
                node.audio.waveformPoints = sn.audioWaveformPoints;
                node.audio.spectrumBars = sn.audioSpectrumBars;
                node.audio.barWidth = sn.audioBarWidth;
                node.audio.barGap = sn.audioBarGap;
                node.audio.barColor = sn.audioBarColor;
                node.audio.backgroundColor = sn.audioBackgroundColor;
            }
            
            graph.AddNode(std::move(node));
        }
        
        // Connections
        for (const auto& sc : data.connections) {
            graph.Connect({sc.fromNodeId, sc.fromPort, sc.toNodeId, sc.toPort});
        }
        
        // Clips
        for (const auto& sc : data.clips) {
            Clip clip;
            clip.clipId = sc.id;
            clip.sourceNodeId = sc.sourceNodeId;
            clip.timelineStart = sc.timelineStart;
            clip.sourceInPoint = sc.sourceInPoint;
            clip.sourceOutPoint = sc.sourceOutPoint;
            clip.playbackSpeed = sc.playbackSpeed;
            clip.layer = sc.layer;
            timeline.AddClip(std::move(clip));
        }
        
        // Transitions
        for (const auto& st : data.transitions) {
            Transition trans;
            trans.fromClipId = st.fromClipId;
            trans.toClipId = st.toClipId;
            trans.duration = st.duration;
            trans.blendShaderNodeId = st.blendShaderNodeId;
            timeline.AddTransition(std::move(trans));
        }
        
        return true;
    } catch (const std::exception& e) {
        LOGE("Deserialize failed: %s", e.what());
        return false;
    }
}

// --- File I/O ---

bool ProjectSerializer::SaveToFile(const ProjectData& data, const std::string& filePath) {
    try {
        std::ofstream file(filePath);
        if (!file) {
            LOGE("Failed to open file for writing: %s", filePath.c_str());
            return false;
        }
        
        // Write as pretty JSON
        file << "{\n";
        file << "  \"version\": " << data.metadata.version << ",\n";
        file << "  \"metadata\": {\n";
        file << "    \"name\": \"" << data.metadata.name << "\",\n";
        file << "    \"version\": " << data.metadata.version << ",\n";
        file << "    \"duration\": " << data.metadata.duration << ",\n";
        file << "    \"frameRate\": " << data.metadata.frameRate << ",\n";
        file << "    \"width\": " << data.metadata.width << ",\n";
        file << "    \"height\": " << data.metadata.height << ",\n";
        file << "    \"createdDate\": \"" << data.metadata.createdDate << "\",\n";
        file << "    \"modifiedDate\": \"" << data.metadata.modifiedDate << "\"\n";
        file << "  },\n";
        
        // Nodes
        file << "  \"nodes\": [\n";
        for (size_t i = 0; i < data.nodes.size(); ++i) {
            const auto& n = data.nodes[i];
            file << "    {\n";
            file << "      \"id\": \"" << n.id << "\",\n";
            file << "      \"type\": \"" << n.type << "\",\n";
            file << "      \"name\": \"" << n.name << "\",\n";
            file << "      \"x\": " << n.x << ",\n";
            file << "      \"y\": " << n.y << ",\n";
            
            // Uniforms
            file << "      \"uniforms\": {\n";
            bool first = true;
            for (const auto& [key, val] : n.uniforms) {
                if (!first) file << ",\n";
                first = false;
                file << "        \"" << key << "\": " << val;
            }
            file << "\n      },\n";
            
            // Animated uniforms
            file << "      \"animatedUniforms\": [\n";
            for (size_t j = 0; j < n.animatedUniforms.size(); ++j) {
                const auto& au = n.animatedUniforms[j];
                file << "        {\n";
                file << "          \"name\": \"" << au.name << "\",\n";
                file << "          \"keyframes\": [\n";
                for (size_t k = 0; k < au.keyframes.size(); ++k) {
                    const auto& kf = au.keyframes[k];
                    file << "            {\n";
                    file << "              \"time\": " << kf.time << ",\n";
                    file << "              \"value\": " << kf.value << ",\n";
                    file << "              \"interpolation\": " << kf.interpolation << ",\n";
                    file << "              \"inTangent\": " << kf.inTangent << ",\n";
                    file << "              \"outTangent\": " << kf.outTangent << "\n";
                    file << "            }";
                    if (k + 1 < au.keyframes.size()) file << ",";
                    file << "\n";
                }
                file << "          ]\n";
                file << "        }";
                if (j + 1 < n.animatedUniforms.size()) file << ",";
                file << "\n";
            }
            file << "      ]\n";
            
            if (!n.sourceFilePath.empty()) {
                file << ",\n      \"sourceFilePath\": \"" << n.sourceFilePath << "\"";
            }
            if (n.blendMode != 0) {
                file << ",\n      \"blendMode\": " << n.blendMode;
            }
            if (n.type == "AudioReactive" || n.type == "AudioWaveform" || n.type == "AudioSpectrum") {
                file << ",\n      \"audio\": {\n";
                file << "        \"sourceClipId\": \"" << n.audioSourceClipId << "\",\n";
                file << "        \"sensitivity\": " << n.audioSensitivity << ",\n";
                file << "        \"smoothing\": " << n.audioSmoothing << ",\n";
                file << "        \"frequencyMin\": " << n.audioFrequencyMin << ",\n";
                file << "        \"frequencyMax\": " << n.audioFrequencyMax << ",\n";
                file << "        \"fftSize\": " << n.audioFftSize << ",\n";
                file << "        \"useBeatDetection\": " << (n.audioUseBeatDetection ? "true" : "false") << ",\n";
                file << "        \"beatThreshold\": " << n.audioBeatThreshold << ",\n";
                file << "        \"waveformPoints\": " << n.audioWaveformPoints << ",\n";
                file << "        \"spectrumBars\": " << n.audioSpectrumBars << ",\n";
                file << "        \"barWidth\": " << n.audioBarWidth << ",\n";
                file << "        \"barGap\": " << n.audioBarGap << ",\n";
                file << "        \"barColor\": " << n.audioBarColor << ",\n";
                file << "        \"backgroundColor\": " << n.audioBackgroundColor << "\n";
                file << "      }";
            }
            
            file << "\n    }";
            if (i + 1 < data.nodes.size()) file << ",";
            file << "\n";
        }
        file << "  ],\n";
        
        // Connections
        file << "  \"connections\": [\n";
        for (size_t i = 0; i < data.connections.size(); ++i) {
            const auto& c = data.connections[i];
            file << "    {\"fromNodeId\": \"" << c.fromNodeId << "\", \"fromPort\": \"" << c.fromPort
                 << "\", \"toNodeId\": \"" << c.toNodeId << "\", \"toPort\": \"" << c.toPort << "\"}";
            if (i + 1 < data.connections.size()) file << ",";
            file << "\n";
        }
        file << "  ],\n";
        
        // Clips
        file << "  \"clips\": [\n";
        for (size_t i = 0; i < data.clips.size(); ++i) {
            const auto& c = data.clips[i];
            file << "    {\"id\": \"" << c.id << "\", \"sourceNodeId\": \"" << c.sourceNodeId
                 << "\", \"timelineStart\": " << c.timelineStart
                 << ", \"sourceInPoint\": " << c.sourceInPoint
                 << ", \"sourceOutPoint\": " << c.sourceOutPoint
                 << ", \"playbackSpeed\": " << c.playbackSpeed
                 << ", \"layer\": " << c.layer << "}";
            if (i + 1 < data.clips.size()) file << ",";
            file << "\n";
        }
        file << "  ],\n";
        
        // Transitions
        file << "  \"transitions\": [\n";
        for (size_t i = 0; i < data.transitions.size(); ++i) {
            const auto& t = data.transitions[i];
            file << "    {\"fromClipId\": \"" << t.fromClipId << "\", \"toClipId\": \"" << t.toClipId
                 << "\", \"duration\": " << t.duration
                 << ", \"blendShaderNodeId\": \"" << t.blendShaderNodeId << "\"}";
            if (i + 1 < data.transitions.size()) file << ",";
            file << "\n";
        }
        file << "  ]\n";
        file << "}\n";
        
        return true;
    } catch (const std::exception& e) {
        LOGE("SaveToFile failed: %s", e.what());
        return false;
    }
}

bool ProjectSerializer::LoadFromFile(ProjectData& data, const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file) {
            LOGE("Failed to open file for reading: %s", filePath.c_str());
            return false;
        }
        
        std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        return JsonDeserialize(json, data);
    } catch (const std::exception& e) {
        LOGE("LoadFromFile failed: %s", e.what());
        return false;
    }
}

std::string ProjectSerializer::GetAutosavePath(const std::string& projectPath) {
    std::filesystem::path p(projectPath);
    return (p.parent_path() / (p.stem().string() + ".autosave" + p.extension().string())).string();
}

bool ProjectSerializer::SaveAutosave(const ProjectData& data, const std::string& projectPath) {
    return SaveToFile(data, GetAutosavePath(projectPath));
}

bool ProjectSerializer::LoadAutosave(ProjectData& data, const std::string& projectPath) {
    return LoadFromFile(data, GetAutosavePath(projectPath));
}

std::vector<std::string> ProjectSerializer::GetRecentProjects() {
    // Would read from SharedPreferences / config file
    return {};
}

void ProjectSerializer::AddRecentProject(const std::string& path) {
    // Would save to SharedPreferences
}

void ProjectSerializer::ClearRecentProjects() {
    // Would clear SharedPreferences
}

// --- ProjectManager Implementation ---

ProjectManager::ProjectManager() {
    lastAutosave_ = std::chrono::steady_clock::now();
}

ProjectManager::~ProjectManager() {
    if (modified_ && autosaveEnabled_) {
        TriggerAutosave();
    }
}

bool ProjectManager::NewProject(const std::string& name) {
    currentProject_ = ProjectData{};
    currentProject_->metadata.name = name;
    currentProject_->metadata.createdDate = ProjectSerializer::GetCurrentTimestamp();
    currentProject_->metadata.modifiedDate = ProjectSerializer::GetCurrentTimestamp();
    projectPath_.clear();
    projectName_ = name;
    modified_ = true;
    NotifyChanged();
    return true;
}

bool ProjectManager::OpenProject(const std::string& filePath) {
    ProjectData data;
    if (!ProjectSerializer::LoadFromFile(data, filePath)) {
        if (onError_) onError_("Failed to load project: " + filePath);
        return false;
    }
    
    currentProject_ = std::move(data);
    projectPath_ = filePath;
    projectName_ = currentProject_->metadata.name;
    modified_ = false;
    NotifyChanged();
    return true;
}

bool ProjectManager::SaveProject() {
    if (projectPath_.empty()) return SaveProjectAs("");
    
    if (currentProject_) {
        currentProject_->metadata.modifiedDate = ProjectSerializer::GetCurrentTimestamp();
        if (ProjectSerializer::SaveToFile(*currentProject_, projectPath_)) {
            modified_ = false;
            NotifyChanged();
            return true;
        }
    }
    
    if (onError_) onError_("Failed to save project");
    return false;
}

bool ProjectManager::SaveProjectAs(const std::string& filePath) {
    if (!currentProject_) return false;
    
    std::string path = filePath.empty() ? projectPath_ : filePath;
    if (path.empty()) {
        if (onError_) onError_("No save path specified");
        return false;
    }
    
    currentProject_->metadata.modifiedDate = ProjectSerializer::GetCurrentTimestamp();
    if (ProjectSerializer::SaveToFile(*currentProject_, path)) {
        projectPath_ = path;
        projectName_ = currentProject_->metadata.name;
        modified_ = false;
        NotifyChanged();
        return true;
    }
    
    if (onError_) onError_("Failed to save project as: " + path);
    return false;
}

void ProjectManager::EnableAutosave(bool enable, int intervalSeconds) {
    autosaveEnabled_ = enable;
    autosaveIntervalSec_ = intervalSeconds;
}

void ProjectManager::TriggerAutosave() {
    if (!autosaveEnabled_ || !currentProject_ || projectPath_.empty()) return;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastAutosave_).count();
    if (elapsed < autosaveIntervalSec_) return;
    
    if (ProjectSerializer::SaveAutosave(*currentProject_, projectPath_)) {
        lastAutosave_ = now;
        LOGI("Autosaved project");
    }
}

bool ProjectManager::HasRecoveryData() const {
    if (projectPath_.empty()) return false;
    return std::filesystem::exists(ProjectSerializer::GetAutosavePath(projectPath_));
}

bool ProjectManager::RecoverProject() {
    if (!HasRecoveryData()) return false;
    
    ProjectData data;
    if (ProjectSerializer::LoadAutosave(data, projectPath_)) {
        currentProject_ = std::move(data);
        modified_ = true;
        NotifyChanged();
        return true;
    }
    return false;
}

void ProjectManager::UpdateTimestamps() {
    if (currentProject_) {
        currentProject_->metadata.modifiedDate = ProjectSerializer::GetCurrentTimestamp();
    }
}

void ProjectManager::NotifyChanged() {
    if (onProjectChanged_) onProjectChanged_(projectPath_);
}

std::string ProjectManager::GetRecentProjectsJson() const {
    // In a real implementation, this would read from a recents file
    // For now, return empty array
    return "[]";
}

} // namespace vfx