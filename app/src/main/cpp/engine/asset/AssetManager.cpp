#include "AssetManager.h"

#include <android/log.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#define LOG_TAG "AssetManager"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace vfx {

namespace {
std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

std::string GetExtension(const std::string& path) {
    auto pos = path.find_last_of('.');
    if (pos == std::string::npos) return "";
    return ToLower(path.substr(pos + 1));
}

AssetType ExtensionToType(const std::string& ext) {
    static const std::unordered_map<std::string, AssetType> map = {
        {"jpg", AssetType::Image}, {"jpeg", AssetType::Image}, {"png", AssetType::Image},
        {"webp", AssetType::Image}, {"bmp", AssetType::Image}, {"gif", AssetType::Image},
        {"mp4", AssetType::Video}, {"mov", AssetType::Video}, {"avi", AssetType::Video},
        {"mkv", AssetType::Video}, {"webm", AssetType::Video}, {"m4v", AssetType::Video},
        {"mp3", AssetType::Audio}, {"wav", AssetType::Audio}, {"aac", AssetType::Audio},
        {"flac", AssetType::Audio}, {"ogg", AssetType::Audio}, {"m4a", AssetType::Audio},
        {"ttf", AssetType::Font}, {"otf", AssetType::Font}, {"woff", AssetType::Font},
        {"gltf", AssetType::Model}, {"glb", AssetType::Model}, {"obj", AssetType::Model},
        {"cube", AssetType::LUT}, {"3dl", AssetType::LUT},
    };
    auto it = map.find(ext);
    return it != map.end() ? it->second : AssetType::Unknown;
}
} // anonymous

AssetManager::AssetManager() = default;

AssetManager::~AssetManager() = default;

std::string AssetManager::GenerateAssetId(const std::string& path) const {
    std::hash<std::string> hasher;
    return "asset_" + std::to_string(hasher(path));
}

AssetType AssetManager::DetectAssetType(const std::string& path) const {
    std::string ext = GetExtension(path);
    AssetType type = ExtensionToType(ext);
    if (type != AssetType::Unknown) return type;
    
    // Try MIME type detection via file header
    std::ifstream f(path, std::ios::binary);
    if (!f) return AssetType::Unknown;
    
    char header[16] = {0};
    f.read(header, sizeof(header));
    
    if (memcmp(header, "\x89PNG", 4) == 0) return AssetType::Image;
    if (memcmp(header, "\xFF\xD8\xFF", 3) == 0) return AssetType::Image;
    if (memcmp(header, "RIFF", 4) == 0 && memcmp(header + 8, "WEBP", 4) == 0) return AssetType::Image;
    if (memcmp(header, "GIF8", 4) == 0) return AssetType::Image;
    if (memcmp(header, "ftyp", 4) == 0) return AssetType::Video;
    if (memcmp(header, "OggS", 4) == 0) return AssetType::Audio;
    if (memcmp(header, "ID3", 3) == 0) return AssetType::Audio;
    if (memcmp(header, "\x00\x00\x00\x20ftypglTF", 12) == 0 || memcmp(header, "glTF", 4) == 0) return AssetType::Model;
    
    return AssetType::Unknown;
}

bool AssetManager::ScanAssetInfo(const std::string& path, AssetInfo& info) {
    std::error_code ec;
    auto fileSize = std::filesystem::file_size(path, ec);
    if (ec) return false;
    info.fileSizeBytes = static_cast<int64_t>(fileSize);
    return true;
}

std::string AssetManager::ImportAsset(const std::string& filePath, AssetType expectedType) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string assetId = GenerateAssetId(filePath);
    if (auto it = assets_.find(assetId); it != assets_.end()) {
        return assetId;
    }
    
    auto info = std::make_unique<AssetInfo>();
    info->assetId = assetId;
    info->sourcePath = filePath;
    info->type = expectedType != AssetType::Unknown ? expectedType : DetectAssetType(filePath);
    
    std::filesystem::path p(filePath);
    info->displayName = p.filename().string();
    
    if (!ScanAssetInfo(filePath, *info)) {
        info->isMissing = true;
        info->errorMessage = "Failed to read asset info";
    }
    
    info->isImported = true;
    
    AssetInfo* ptr = info.get();
    assets_[assetId] = std::move(info);
    
    if (onAssetImported_) onAssetImported_(*ptr);
    LOGI("Imported asset: %s (%s)", assetId.c_str(), filePath.c_str());
    return assetId;
}

bool AssetManager::RemoveAsset(const std::string& assetId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return assets_.erase(assetId) > 0;
}

AssetInfo* AssetManager::GetAsset(const std::string& assetId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = assets_.find(assetId);
    return it != assets_.end() ? it->second.get() : nullptr;
}

const AssetInfo* AssetManager::GetAsset(const std::string& assetId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = assets_.find(assetId);
    return it != assets_.end() ? it->second.get() : nullptr;
}

std::vector<AssetInfo*> AssetManager::GetAllAssets() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AssetInfo*> result;
    result.reserve(assets_.size());
    for (auto& [id, ptr] : assets_) result.push_back(ptr.get());
    return result;
}

std::vector<const AssetInfo*> AssetManager::GetAllAssets() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<const AssetInfo*> result;
    result.reserve(assets_.size());
    for (const auto& [id, ptr] : assets_) result.push_back(ptr.get());
    return result;
}

bool AssetManager::GenerateProxy(const std::string& assetId) {
    AssetInfo* info = GetAsset(assetId);
    if (!info) return false;
    
    info->proxyStatus = ProxyStatus::Pending;
    LOGI("Proxy generation requested for %s (not fully implemented)", assetId.c_str());
    return true;
}

bool AssetManager::IsProxyReady(const std::string& assetId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = assets_.find(assetId);
    if (it == assets_.end()) return false;
    return it->second->proxyStatus == ProxyStatus::Ready;
}

std::string AssetManager::GetProxyPath(const std::string& assetId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = assets_.find(assetId);
    if (it == assets_.end()) return "";
    return it->second->proxyPath;
}

bool AssetManager::GenerateThumbnail(const std::string& assetId) {
    AssetInfo* info = GetAsset(assetId);
    if (!info) return false;
    
    LOGI("Thumbnail generation requested for %s", assetId.c_str());
    return true;
}

std::string AssetManager::GetThumbnailPath(const std::string& assetId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = assets_.find(assetId);
    if (it == assets_.end()) return "";
    return it->second->thumbnailPath;
}

std::vector<AssetInfo*> AssetManager::Search(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AssetInfo*> result;
    std::string lowerQuery = ToLower(query);
    
    for (auto& [id, ptr] : assets_) {
        if (ToLower(ptr->displayName).find(lowerQuery) != std::string::npos ||
            ToLower(ptr->sourcePath).find(lowerQuery) != std::string::npos) {
            result.push_back(ptr.get());
        }
    }
    return result;
}

std::vector<AssetInfo*> AssetManager::GetByType(AssetType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AssetInfo*> result;
    for (auto& [id, ptr] : assets_) {
        if (ptr->type == type) result.push_back(ptr.get());
    }
    return result;
}

std::vector<AssetInfo*> AssetManager::GetMissingAssets() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AssetInfo*> result;
    for (auto& [id, ptr] : assets_) {
        if (ptr->isMissing) result.push_back(ptr.get());
    }
    return result;
}

void AssetManager::ClearMissingAssets() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = assets_.begin(); it != assets_.end(); ) {
        if (it->second->isMissing) {
            it = assets_.erase(it);
        } else {
            ++it;
        }
    }
}

bool AssetManager::SaveDatabase(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream file(filePath);
    if (!file) return false;
    
    file << "{\n  \"version\": 1,\n  \"assets\": [\n";
    bool first = true;
    for (const auto& [id, ptr] : assets_) {
        if (!first) file << ",\n";
        first = false;
        file << "    {\n";
        file << "      \"id\": \"" << ptr->assetId << "\",\n";
        file << "      \"sourcePath\": \"" << ptr->sourcePath << "\",\n";
        file << "      \"type\": " << static_cast<int>(ptr->type) << ",\n";
        file << "      \"displayName\": \"" << ptr->displayName << "\",\n";
        file << "      \"fileSizeBytes\": " << ptr->fileSizeBytes << ",\n";
        file << "      \"width\": " << ptr->width << ",\n";
        file << "      \"height\": " << ptr->height << ",\n";
        file << "      \"frameCount\": " << ptr->frameCount << ",\n";
        file << "      \"frameRate\": " << ptr->frameRate << ",\n";
        file << "      \"durationUs\": " << ptr->durationUs << ",\n";
        file << "      \"proxyStatus\": " << static_cast<int>(ptr->proxyStatus) << ",\n";
        file << "      \"proxyPath\": \"" << ptr->proxyPath << "\",\n";
        file << "      \"proxyWidth\": " << ptr->proxyWidth << ",\n";
        file << "      \"proxyHeight\": " << ptr->proxyHeight << ",\n";
        file << "      \"isMissing\": " << (ptr->isMissing ? "true" : "false") << "\n";
        file << "    }";
    }
    file << "\n  ]\n}\n";
    return true;
}

bool AssetManager::LoadDatabase(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file) return false;
    
    std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    std::lock_guard<std::mutex> lock(mutex_);
    assets_.clear();
    
    size_t assetsStart = json.find("\"assets\"");
    if (assetsStart == std::string::npos) return false;
    
    size_t arrStart = json.find('[', assetsStart);
    if (arrStart == std::string::npos) return false;
    
    size_t arrEnd = json.find(']', arrStart);
    std::string assetsJson = json.substr(arrStart, arrEnd - arrStart + 1);
    
    size_t pos = 0;
    while (true) {
        size_t objStart = assetsJson.find("{", pos);
        if (objStart == std::string::npos) break;
        size_t objEnd = assetsJson.find("}", objStart);
        if (objEnd == std::string::npos) break;
        
        std::string obj = assetsJson.substr(objStart, objEnd - objStart + 1);
        auto info = std::make_unique<AssetInfo>();
        
        auto getStr = [&](const std::string& key) -> std::string {
            size_t kp = obj.find("\"" + key + "\"");
            if (kp == std::string::npos) return "";
            kp = obj.find(':', kp);
            if (kp == std::string::npos) return "";
            kp = obj.find('"', kp + 1);
            if (kp == std::string::npos) return "";
            size_t end = obj.find('"', kp + 1);
            if (end == std::string::npos) return "";
            return obj.substr(kp + 1, end - kp - 1);
        };
        
        auto getNum = [&](const std::string& key) -> double {
            size_t kp = obj.find("\"" + key + "\"");
            if (kp == std::string::npos) return 0.0;
            kp = obj.find(':', kp);
            if (kp == std::string::npos) return 0.0;
            kp++;
            while (kp < obj.size() && isspace(obj[kp])) kp++;
            size_t end = kp;
            while (end < obj.size() && (isdigit(obj[end]) || obj[end] == '.' || obj[end] == '-')) end++;
            return std::stod(obj.substr(kp, end - kp));
        };
        
        info->assetId = getStr("id");
        info->sourcePath = getStr("sourcePath");
        info->type = static_cast<AssetType>(static_cast<int>(getNum("type")));
        info->displayName = getStr("displayName");
        info->fileSizeBytes = static_cast<int64_t>(getNum("fileSizeBytes"));
        info->width = static_cast<int>(getNum("width"));
        info->height = static_cast<int>(getNum("height"));
        info->frameCount = static_cast<int>(getNum("frameCount"));
        info->frameRate = getNum("frameRate");
        info->durationUs = static_cast<int64_t>(getNum("durationUs"));
        info->proxyStatus = static_cast<ProxyStatus>(static_cast<int>(getNum("proxyStatus")));
        info->proxyPath = getStr("proxyPath");
        info->proxyWidth = static_cast<int>(getNum("proxyWidth"));
        info->proxyHeight = static_cast<int>(getNum("proxyHeight"));
        info->isMissing = obj.find("\"isMissing\": true") != std::string::npos;
        info->isImported = true;
        
        assets_[info->assetId] = std::move(info);
        pos = objEnd + 1;
    }
    
    LOGI("Loaded asset database: %zu assets", assets_.size());
    return true;
}

} // namespace vfx
