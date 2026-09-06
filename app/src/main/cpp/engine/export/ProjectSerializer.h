#pragma once
// Phase 6: Project serialization - save/load project files.
// JSON-based format for node graph, timeline, clips, and settings.

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "engine/graph/Node.h"
#include "engine/graph/RenderGraph.h"
#include "engine/timeline/Timeline.h"

namespace vfx {

// Project file version
constexpr int kProjectFileVersion = 1;

// Serializable node data
struct SerializedNode {
    std::string id;
    std::string type; // matches NodeKind enum names
    std::string name;
    float x = 0.0f, y = 0.0f;
    
    // Ports (for reconstruction)
    std::vector<std::string> inputPorts;
    std::vector<std::string> outputPorts;
    
    // Static uniforms
    std::unordered_map<std::string, float> uniforms;
    
    // Animated uniforms (keyframes only - no std::function)
    struct AnimatedUniform {
        std::string name;
        struct Keyframe {
            double time = 0.0;
            float value = 0.0f;
            int interpolation = 0; // InterpolationType enum
            float inTangent = 0.0f;
            float outTangent = 0.0f;
        };
        std::vector<Keyframe> keyframes;
    };
    std::vector<AnimatedUniform> animatedUniforms;
    
    // Node-specific data
    std::string sourceFilePath;     // For VideoSource/ImageSource
    int blendMode = 0;              // For Blend
    std::vector<uint32_t> spirvFragment; // For Shader
};

// Serializable connection
struct SerializedConnection {
    std::string fromNodeId;
    std::string fromPort;
    std::string toNodeId;
    std::string toPort;
};

// Serializable clip
struct SerializedClip {
    std::string id;
    std::string sourceNodeId;
    double timelineStart = 0.0;
    double sourceInPoint = 0.0;
    double sourceOutPoint = 0.0;
    double playbackSpeed = 1.0;
    int layer = 0;
    int color = 0xFF2196F3;
};

// Serializable transition
struct SerializedTransition {
    std::string fromClipId;
    std::string toClipId;
    double duration = 0.0;
    std::string blendShaderNodeId;
};

// Project metadata
struct ProjectMetadata {
    std::string name = "Untitled Project";
    std::string author;
    std::string createdDate;
    std::string modifiedDate;
    int version = kProjectFileVersion;
    double duration = 10.0;
    double frameRate = 30.0;
    uint32_t width = 1920;
    uint32_t height = 1080;
};

// Complete project data
struct ProjectData {
    ProjectMetadata metadata;
    std::vector<SerializedNode> nodes;
    std::vector<SerializedConnection> connections;
    std::vector<SerializedClip> clips;
    std::vector<SerializedTransition> transitions;
    // Settings would go here
};

/**
 * ProjectSerializer: Handles project save/load operations.
 * Uses JSON format (nlohmann/json style but with minimal deps).
 */
class ProjectSerializer {
public:
    ProjectSerializer() = default;
    ~ProjectSerializer() = default;
    
    // Serialize current engine state to ProjectData
    static ProjectData Serialize(const NodeGraph& graph, const Timeline& timeline);
    
    // Deserialize ProjectData to engine state
    static bool Deserialize(const ProjectData& data, NodeGraph& graph, Timeline& timeline);
    
    // Save to file (JSON)
    static bool SaveToFile(const ProjectData& data, const std::string& filePath);
    
    // Load from file (JSON)
    static bool LoadFromFile(ProjectData& data, const std::string& filePath);
    
    // Auto-save support
    static std::string GetAutosavePath(const std::string& projectPath);
    static bool SaveAutosave(const ProjectData& data, const std::string& projectPath);
    static bool LoadAutosave(ProjectData& data, const std::string& projectPath);
    
    // Recent projects management
    static std::vector<std::string> GetRecentProjects();
    static void AddRecentProject(const std::string& path);
    static void ClearRecentProjects();

private:
    // JSON helpers (minimal implementation without nlohmann/json dependency)
    struct JsonValue;
    static std::string JsonSerialize(const ProjectData& data);
    static bool JsonDeserialize(const std::string& json, ProjectData& data);
    
    static void WriteJsonValue(std::ostream& os, const JsonValue& value, int indent = 0);
    static bool ReadJsonValue(std::istream& is, JsonValue& value);
    
    // NodeKind <-> string conversion
    static std::string NodeKindToString(NodeKind kind);
    static NodeKind StringToNodeKind(const std::string& str);
    static std::string BlendModeToString(BlendMode mode);
    static BlendMode StringToBlendMode(const std::string& str);
    static std::string InterpolationTypeToString(int type);
    static int StringToInterpolationType(const std::string& str);
};

/**
 * ProjectManager: High-level project lifecycle management.
 * Handles new/open/save/save-as, autosave timer, and crash recovery.
 */
class ProjectManager {
public:
    ProjectManager();
    ~ProjectManager();
    
    // Project operations
    bool NewProject(const std::string& name = "Untitled");
    bool OpenProject(const std::string& filePath);
    bool SaveProject();
    bool SaveProjectAs(const std::string& filePath);
    
    // Current project state
    [[nodiscard]] bool HasProject() const { return currentProject_.has_value(); }
    [[nodiscard]] const std::string& GetProjectPath() const { return projectPath_; }
    [[nodiscard]] const std::string& GetProjectName() const { return projectName_; }
    [[nodiscard]] bool IsModified() const { return modified_; }
    
    // Access to project data for engine sync
    [[nodiscard]] const ProjectData& GetData() const { return projectData_; }
    [[nodiscard]] ProjectData& GetMutableData() { return projectData_; }
    
    // Mark project as modified
    void SetModified(bool modified = true) { modified_ = modified; }
    
    // Autosave
    void EnableAutosave(bool enable, int intervalSeconds = 60);
    void TriggerAutosave();
    
    // Crash recovery
    bool HasRecoveryData() const;
    bool RecoverProject();
    
    // Callbacks
    using ProjectCallback = std::function<void(const std::string& projectPath)>;
    void SetOnProjectChanged(ProjectCallback cb) { onProjectChanged_ = std::move(cb); }
    void SetOnError(std::function<void(const std::string&)> cb) { onError_ = std::move(cb); }

private:
    std::optional<ProjectData> currentProject_;
    std::string projectPath_;
    std::string projectName_;
    bool modified_ = false;
    
    bool autosaveEnabled_ = true;
    int autosaveIntervalSec_ = 60;
    std::chrono::steady_clock::time_point lastAutosave_;
    
    ProjectCallback onProjectChanged_;
    std::function<void(const std::string&)> onError_;
    
    void UpdateTimestamps();
    void NotifyChanged();
};

} // namespace vfx