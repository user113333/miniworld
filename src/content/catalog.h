#pragma once
#include <glm/glm.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace content {
enum class AssetType { folder, image, font, source, document, unsupported };
enum class LoadStatus { unloaded, loaded, unsupported, failed };
struct Asset {
    std::string key; // Normalized, case-sensitive path relative to the resource root.
    std::string name;
    AssetType type = AssetType::unsupported;
    std::uintmax_t size = 0;
    std::string loader;
    LoadStatus status = LoadStatus::unsupported;
    std::string diagnostic;
    glm::ivec2 dimensions{};
};
const char* type_name(AssetType type);
const char* status_name(LoadStatus status);
bool valid_key(const std::string& key);

// Catalog metadata never owns SDL or ImGui objects. Scan once before consumers attach.
class Catalog {
public:
    bool initialize(const std::filesystem::path& root);
    const std::filesystem::path& root() const { return root_; }
    const std::vector<Asset>& assets() const { return assets_; }
    const Asset* find(const std::string& key) const;
    std::optional<std::filesystem::path> path(const std::string& key) const;
    std::optional<std::vector<unsigned char>> font_data(const std::string& key);
    void report(const std::string& key, glm::ivec2 dimensions, const std::string& error = {});
    const std::string& error() const { return error_; }
private:
    std::filesystem::path root_;
    std::vector<Asset> assets_;
    std::unordered_map<std::string, std::size_t> by_key_;
    std::string error_;
};
}
