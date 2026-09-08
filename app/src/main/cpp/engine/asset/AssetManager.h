#pragma once
// Asset Management: media database, proxy workflow, thumbnail generation.
// Tracks imported media files, generates proxies for smooth editing, and
// provides metadata lookup for the node editor.

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <filesystem>

namespace vfx {

enum class AssetType {
    Unknown,
    Image,
    Video,
    Audio,
    Font,
    Shader,
    Model,      // glTF/OBJ
    Sequence,   // Image sequence
    LUT         // 3D LUT
};

enum class ProxyStatus {
    None,
    Pending,
    Ready,
    Failed
};

struct AssetInfo {
    std::string assetId;
    std::string sourcePath;
    AssetType type = AssetType::Unknown;
    int64_t fileSizeBytes = 0;
    std::string mimeType;
    std::string displayName;
    
    // Image/Video specific
    int width = 0;
    int height = 0;
    int frameCount = 0;
    double frameRate = 30.0;
    int64_t durationUs = 0;
    std::string videoCodec;
    std::string audioCodec;
    
    // Audio specific
    int audioChannels = 0;
    int audioSampleRate = 0;
    int64_t audioDurationUs = 0;
    
    // Proxy
    std::string proxyPath;
    ProxyStatus proxyStatus = ProxyStatus::None;
    int proxyWidth = 0;
    int proxyHeight = 0;
    
    // Thumbnail
    std::string thumbnailPath;
    bool hasThumbnail = false;
    
    // Status
    bool isImported = false;
    bool isMissing = false;
    std::string errorMessage;
};

struct ProxyConfig {
    int maxWidth = 1280;
    int maxHeight = 720;
    int imageQuality = 85;       // JPEG quality 1-100
    int videoBitrateKbps = 2000;
    std::string videoCodec = "video/avc";
    bool generateThumbnail = true;
    int thumbnailSize = 256;
};

class AssetManager {
public:
    AssetManager();
    ~AssetManager();
    
    // Asset database
    std::string ImportAsset(const std::string& filePath, AssetType expectedType = AssetType::Unknown);
    bool RemoveAsset(const std::string& assetId);
    AssetInfo* GetAsset(const std::string& assetId);
    const AssetInfo* GetAsset(const std::string& assetId) const;
    std::vector<AssetInfo*> GetAllAssets();
    std::vector<const AssetInfo*> GetAllAssets() const;
    
    // Proxy generation
    void SetProxyConfig(const ProxyConfig& config) { proxyConfig_ = config; }
    [[nodiscard]] const ProxyConfig& GetProxyConfig() const { return proxyConfig_; }
    
    bool GenerateProxy(const std::string& assetId);
    bool IsProxyReady(const std::string& assetId) const;
    std::string GetProxyPath(const std::string& assetId) const;
    
    // Thumbnail
    bool GenerateThumbnail(const std::string& assetId);
    std::string GetThumbnailPath(const std::string& assetId) const;
    
    // Query
    std::vector<AssetInfo*> Search(const std::string& query);
    std::vector<AssetInfo*> GetByType(AssetType type);
    std::vector<AssetInfo*> GetMissingAssets();
    
    // Database persistence
    bool SaveDatabase(const std::string& filePath);
    bool LoadDatabase(const std::string& filePath);
    
    // Cleanup
    void ClearMissingAssets();
    size_t GetAssetCount() const { return assets_.size(); }
    
    // Events
    using AssetCallback = std::function<void(const AssetInfo&)>;
    void SetOnAssetImported(AssetCallback cb) { onAssetImported_ = std::move(cb); }
    void SetOnProxyReady(AssetCallback cb) { onProxyReady_ = std::move(cb); }
    
private:
    std::string GenerateAssetId(const std::string& path) const;
    AssetType DetectAssetType(const std::string& path) const;
    bool ScanAssetInfo(const std::string& path, AssetInfo& info);
    
    std::unordered_map<std::string, std::unique_ptr<AssetInfo>> assets_;
    ProxyConfig proxyConfig_;
    mutable std::mutex mutex_;
    
    AssetCallback onAssetImported_;
    AssetCallback onProxyReady_;
    
    std::string databasePath_;
};

} // namespace vfx
