#include "catalog.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>

namespace content {
const char* type_name(AssetType type)
{
    switch (type) {
    case AssetType::folder: return "Folder";
    case AssetType::image: return "PNG image";
    case AssetType::font: return "Font";
    case AssetType::source: return "Aseprite source";
    case AssetType::document: return "Document/license";
    default: return "Unsupported";
    }
}
const char* status_name(LoadStatus status)
{
    switch (status) {
    case LoadStatus::unloaded: return "Not loaded";
    case LoadStatus::loaded: return "Loaded";
    case LoadStatus::failed: return "Failed";
    default: return "Unsupported";
    }
}
bool valid_key(const std::string& key)
{
    if (key.empty() || key.find('\\') != std::string::npos || key.find(':') != std::string::npos) return false;
    const std::filesystem::path path(key);
    if (path.is_absolute() || path.generic_string() != key || path.lexically_normal().generic_string() != key) return false;
    for (const auto& part : path) if (part == ".." || part == "." || part.empty()) return false;
    return true;
}

bool Catalog::initialize(const std::filesystem::path& root)
{
    assets_.clear(); by_key_.clear(); error_.clear();
    std::error_code ec;
    root_ = std::filesystem::canonical(root, ec);
    if (ec || !std::filesystem::is_directory(root_)) {
        error_ = "Cannot open resource root " + root.string() + ": " + (ec ? ec.message() : "not a directory"); return false;
    }
    std::filesystem::recursive_directory_iterator it(root_, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        // Never follow links outside the staged content root.
        if (it->is_symlink(ec)) { it.disable_recursion_pending(); continue; }
        const bool folder = it->is_directory(ec);
        if (ec) break;
        if (!folder && !it->is_regular_file(ec)) continue;
        Asset asset;
        asset.key = it->path().lexically_relative(root_).generic_string();
        asset.name = it->path().filename().string();
        if (!valid_key(asset.key)) { error_ = "Invalid asset key: " + asset.key; return false; }
        std::string ext = it->path().extension().string();
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (folder) { asset.type = AssetType::folder; asset.loader = "Navigation"; }
        else if (ext == ".png") { asset.type = AssetType::image; asset.loader = "SDL PNG"; asset.status = LoadStatus::unloaded; }
        else if (ext == ".ttf") { asset.type = AssetType::font; asset.loader = "Font bytes"; asset.status = LoadStatus::unloaded; }
        else if (ext == ".aseprite") { asset.type = AssetType::source; asset.loader = "cute Aseprite animation"; asset.diagnostic = "No static preview; runtime animation loading is available."; }
        else if (ext == ".md" || ext == ".txt" || asset.name.find("LICENSE") != std::string::npos) {
            asset.type = AssetType::document; asset.diagnostic = "Metadata only; no document preview.";
        } else asset.diagnostic = "No runtime loader for this file format.";
        if (!folder) {
            asset.size = it->file_size(ec);
            if (ec) break;
        }
        assets_.push_back(std::move(asset));
    }
    if (ec) { error_ = "Cannot scan " + root_.string() + ": " + ec.message(); return false; }
    std::sort(assets_.begin(), assets_.end(), [](const Asset& a, const Asset& b) { return a.key < b.key; });
    for (std::size_t i = 0; i < assets_.size(); ++i) {
        if (!by_key_.emplace(assets_[i].key, i).second) {
            error_ = "Duplicate asset key: " + assets_[i].key; return false;
        }
    }
    return true;
}
const Asset* Catalog::find(const std::string& key) const
{
    if (!valid_key(key)) return nullptr;
    const auto found = by_key_.find(key);
    return found == by_key_.end() ? nullptr : &assets_[found->second];
}
std::optional<std::filesystem::path> Catalog::path(const std::string& key) const
{
    if (!find(key)) return std::nullopt;
    return root_ / std::filesystem::path(key);
}
void Catalog::report(const std::string& key, glm::ivec2 dimensions, const std::string& error)
{
    const auto found = by_key_.find(key);
    if (found == by_key_.end()) return;
    auto& asset = assets_[found->second];
    asset.dimensions = dimensions;
    asset.status = error.empty() ? LoadStatus::loaded : LoadStatus::failed;
    asset.diagnostic = error.empty() ? "" : key + ": " + error;
}
std::optional<std::vector<unsigned char>> Catalog::font_data(const std::string& key)
{
    const auto* asset = find(key);
    if (!asset || asset->type != AssetType::font) { error_ = "Missing or invalid font asset: " + key; return std::nullopt; }
    std::ifstream file(*path(key), std::ios::binary | std::ios::ate);
    const auto size = file ? file.tellg() : std::streampos{-1};
    if (size <= 0 || size > std::numeric_limits<int>::max()) {
        report(key, {}, "Cannot read font data or file is empty/too large"); error_ = find(key)->diagnostic; return std::nullopt;
    }
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        report(key, {}, "Cannot read font data"); error_ = find(key)->diagnostic; return std::nullopt;
    }
    report(key, {}); return bytes;
}
}
