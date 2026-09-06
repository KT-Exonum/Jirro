#include "ProjectSerializer.h"

#include <android/log.h>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
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
    // Simplified JSON parser - in production use nlohmann/json or similar
    // This is a minimal implementation for basic types
    char c;
    while (is >> c) {
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        
        if (c == '{') {
            value.type = JsonValue::Object;
            while (is >> c) {
                if (c == '}') break;
                if (c == ',') continue;
                if (c == ' ' || c == '\n') continue;
                
                std::string key;
                if (c == '"') {
                    std::getline(is, key, '"');
                }
                
                is >> c; // skip :
                
                JsonValue val;
                ReadJsonValue(is, val);
                value.objVal[key] = std::move(val);
            }
            return true;
        }
        else if (c == '[') {
            value.type = JsonValue::Array;
            while (is >> c) {
                if (c == ']') break;
                if (c == ',') continue;
                if (c == ' ' || c == '\n') { is.unget(); continue; }
                
                is.unget();
                JsonValue val;
                ReadJsonValue(is, val);
                value.arrVal.push_back(std::move(val));
            }
            return true;
        }
        else if (c == '"') {
            std::getline(is, value.strVal, '"');
            value.type = JsonValue::String;
            return true;
        }
        else if (c == 't' || c == 'f') {
            std::string val(1, c);
            char next;
            while (is >> next && next != ' ' && next != ',' && next != '}' && next != ']') {
                val += next;
            }
            value.type = JsonValue::Bool;
            value.boolVal = (val == "true");
            return true;
        }
        else if (c == 'n') {
            // null
            char buf[4];
            is.read(buf, 3);
            value.type = JsonValue::Null;
            return true;
        }
        else {
            // Number
            std::string num(1, c);
            char next;
            while (is >> next && (isdigit(next) || next == '.' || next == '-' || next == 'e' || next == 'E')) {
                num += next;
            }
            is.unget();
            value.type = JsonValue::Number;
            value.numVal = std::stod(num);
            return true;
        }
    }
    return false;
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
    if (str == "ShapeStroke") return NodeKind::ShapeStroke;
    if (str == "ShapeFill") return NodeKind::ShapeFill;
    if (str == "ShapeRepeater") return NodeKind::ShapeRepeater;
    if (str == "ShapeBoolean") return NodeKind::ShapeBoolean;
    if (str == "Transform3D") return NodeKind::Transform3D;
    if (str == "Camera3D") return NodeKind::Camera3D;
    if (str == "DepthOfField") return NodeKind::DepthOfField;
    if (str == "ChromaKey") return NodeKind::ChromaKey;
    if (str == "MeshSource") return NodeKind::MeshSource;
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
static std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
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
        sn.outputPorts.push_back(node.output.slotName);
        
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
        graph = NodeGraph{};
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
                node.output = {portName};
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
                    kfData.inTangent = {0, kf.inTangent};
                    kfData.outTangent = {0, kf.outTangent};
                    track.AddKeyframe(kfData);
                }
                node.animatedUniforms[au.name] = std::move(track);
            }
            
            // Node-specific
            node.sourceFilePath = sn.sourceFilePath;
            node.blendMode = static_cast<BlendMode>(sn.blendMode);
            node.spirvFragment = sn.spirvFragment;
            
            graph.AddNode(std::move(node));
        }
        
        // Connections
        for (const auto& sc : data.connections) {
            graph.Connect({sc.fromNodeId, sc.fromPort, sc.toNodeId, sc.toPort});
        }
        
        // Clips
        for (const auto& sc : data.clips) {
            Timeline::Clip clip;
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
            Timeline::Transition trans;
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
        
        // In production, use a proper JSON parser
        // This is a minimal implementation - real code would parse properly
        LOGI("LoadFromFile: basic implementation - use nlohmann/json for production");
        return false;
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
    currentProject_->metadata.createdDate = GetCurrentTimestamp();
    currentProject_->metadata.modifiedDate = GetCurrentTimestamp();
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
        currentProject_->metadata.modifiedDate = GetCurrentTimestamp();
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
    
    currentProject_->metadata.modifiedDate = GetCurrentTimestamp();
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
        currentProject_->metadata.modifiedDate = GetCurrentTimestamp();
    }
}

void ProjectManager::NotifyChanged() {
    if (onProjectChanged_) onProjectChanged_(projectPath_);
}

} // namespace vfx