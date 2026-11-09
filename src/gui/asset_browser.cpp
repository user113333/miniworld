#include "asset_browser.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cmath>

namespace {
std::string lower(std::string value)
{
    for (auto& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
void preview(const CachedImage& image, ImVec2 minimum, ImVec2 size)
{
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 maximum{minimum.x + size.x, minimum.y + size.y};
    const float tile = 8 * ImGui::GetStyle().FontScaleDpi;
    for (int y = 0; y * tile < size.y; ++y) for (int x = 0; x * tile < size.x; ++x) {
        draw->AddRectFilled({minimum.x + x * tile, minimum.y + y * tile},
            {std::min(maximum.x, minimum.x + (x + 1) * tile), std::min(maximum.y, minimum.y + (y + 1) * tile)},
            (x + y) % 2 ? IM_COL32(65, 65, 69, 255) : IM_COL32(44, 44, 48, 255));
    }
    const float scale = std::min(size.x / image.mask.size.x, size.y / image.mask.size.y);
    const ImVec2 extent{image.mask.size.x * scale, image.mask.size.y * scale};
    const ImVec2 anchor{std::round(minimum.x + (size.x - extent.x) / 2), std::round(minimum.y + (size.y - extent.y) / 2)};
    // Draw-list vertex colors are white. Preview never changes shared SDL state.
    draw->AddImage(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(image.texture))),
                   anchor, {anchor.x + extent.x, anchor.y + extent.y});
}
const char* icon(content::AssetType type)
{
    switch (type) {
    case content::AssetType::folder: return "FOLDER";
    case content::AssetType::font: return "FONT";
    case content::AssetType::source: return "SOURCE";
    case content::AssetType::document: return "DOC";
    default: return "FILE";
    }
}
}

void AssetBrowser::draw(content::Catalog& catalog, TextureCache& textures)
{
    if (!open_) return;
    const float dpi = ImGui::GetStyle().FontScaleDpi;
    ImGui::SetNextWindowSize({940 * dpi, 640 * dpi}, ImGuiCond_FirstUseEver);
    if (focus_) { ImGui::SetNextWindowFocus(); focus_ = false; }
    if (!ImGui::Begin("Assets", &open_)) { ImGui::End(); return; }
    if (ImGui::Button("Root")) folder_.clear();
    ImGui::SameLine();
    ImGui::BeginDisabled(folder_.empty());
    if (ImGui::Button("Up")) folder_ = std::filesystem::path(folder_).parent_path().generic_string();
    ImGui::EndDisabled();
    ImGui::SameLine(); ImGui::TextUnformatted(folder_.empty() ? "res/" : folder_.c_str());
    ImGui::InputTextWithHint("Search", "Name/path in all folders", search_, sizeof(search_));
    ImGui::SetNextItemWidth(165 * dpi);
    ImGui::Combo("Type", &type_, "All types\0PNG image\0Font\0Aseprite source\0Document/license\0Unsupported\0");
    ImGui::SameLine(); ImGui::SetNextItemWidth(130 * dpi);
    ImGui::Combo("Sort", &sort_, "Name\0Type\0");
    ImGui::SetNextItemWidth(200 * dpi);
    ImGui::SliderFloat("Thumbnail size", &thumbnail_size_, 48, 160, "%.0f px", ImGuiSliderFlags_AlwaysClamp);
    std::vector<const content::Asset*> items;
    const auto query = lower(search_);
    for (const auto& asset : catalog.assets()) {
        if (query.empty() && std::filesystem::path(asset.key).parent_path().generic_string() != folder_) continue;
        if (!query.empty() && lower(asset.key).find(query) == std::string::npos) continue;
        if (type_ && asset.type != content::AssetType::folder && static_cast<int>(asset.type) != type_) continue;
        items.push_back(&asset);
    }
    std::sort(items.begin(), items.end(), [&](auto a, auto b) {
        if ((a->type == content::AssetType::folder) != (b->type == content::AssetType::folder))
            return a->type == content::AssetType::folder;
        if (sort_ && a->type != b->type) return a->type < b->type;
        const auto a_name = lower(a->name), b_name = lower(b->name);
        return a_name == b_name ? a->key < b->key : a_name < b_name;
    });
    const auto selected = std::find_if(items.begin(), items.end(), [&](auto asset) { return asset->key == selected_; });
    const bool hidden = !selected_.empty() && selected == items.end();
    ImGui::Text("%zu items / %zu catalog entries", items.size(), catalog.assets().size());
    const float detail_width = std::min(300 * dpi, ImGui::GetContentRegionAvail().x * 0.4f);
    const float grid_width = std::max(1.0f, ImGui::GetContentRegionAvail().x - detail_width - ImGui::GetStyle().ItemSpacing.x);
    ImGui::BeginChild("Grid", {grid_width, 0}, ImGuiChildFlags_Borders);
    const float cell_width = std::max(120.0f, thumbnail_size_ + 16) * dpi;
    const float cell_height = (thumbnail_size_ + 16) * dpi + ImGui::GetTextLineHeightWithSpacing() * 2;
    const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cell_width));
    const int rows = (static_cast<int>(items.size()) + columns - 1) / columns;
    int index = selected == items.end() ? -1 : static_cast<int>(selected - items.begin());
    bool focus_item = false;
    if (ImGui::IsWindowFocused() && !ImGui::GetIO().WantTextInput && !items.empty()) {
        int next = index;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) ++next;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) --next;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) next += columns;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) next -= columns;
        if (ImGui::IsKeyPressed(ImGuiKey_Home)) next = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_End)) next = static_cast<int>(items.size()) - 1;
        if (next != index) {
            index = std::clamp(next, 0, static_cast<int>(items.size()) - 1);
            selected_ = items[index]->key; focus_item = true;
        }
    }
    const ImVec2 start = ImGui::GetCursorScreenPos();
    std::string navigate;
    ImGuiListClipper clipper;
    clipper.Begin(rows, cell_height);
    if (focus_item) clipper.IncludeItemByIndex(index / columns);
    while (clipper.Step()) for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        for (int col = 0; col < columns; ++col) {
            const int i = row * columns + col;
            if (i >= static_cast<int>(items.size())) break;
            const auto& asset = *items[i];
            const ImVec2 pos{start.x + col * cell_width, start.y + row * cell_height};
            ImGui::SetCursorScreenPos(pos);
            ImGui::PushID(asset.key.c_str());
            if (ImGui::Selectable("##asset", selected_ == asset.key, ImGuiSelectableFlags_AllowDoubleClick,
                                  {cell_width - 8 * dpi, cell_height - 8 * dpi})) selected_ = asset.key;
            if (focus_item && i == index) { ImGui::SetKeyboardFocusHere(-1); ImGui::SetScrollHereY(); }
            const bool hovered = ImGui::IsItemHovered();
            if (hovered) ImGui::SetTooltip("%s\n%s", asset.key.c_str(), content::type_name(asset.type));
            if (asset.type == content::AssetType::folder && selected_ == asset.key &&
                ((hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) ||
                 (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)))) navigate = asset.key;
            const ImVec2 minimum{pos.x + 4 * dpi, pos.y + 4 * dpi};
            const ImVec2 thumb{thumbnail_size_ * dpi, thumbnail_size_ * dpi};
            const auto* image = asset.type == content::AssetType::image ? textures.load(asset.key) : nullptr;
            if (image) preview(*image, minimum, thumb);
            else ImGui::GetWindowDrawList()->AddText({minimum.x + 4 * dpi, minimum.y + thumb.y / 2},
                ImGui::GetColorU32(ImGuiCol_TextDisabled), asset.status == content::LoadStatus::failed ? "ERROR" : icon(asset.type));
            ImGui::GetWindowDrawList()->PushClipRect(pos, {pos.x + cell_width - 8 * dpi, pos.y + cell_height}, true);
            ImGui::GetWindowDrawList()->AddText({minimum.x, minimum.y + thumb.y + 4 * dpi},
                ImGui::GetColorU32(ImGuiCol_Text), asset.name.c_str());
            ImGui::GetWindowDrawList()->PopClipRect();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    if (!navigate.empty()) folder_ = navigate;
    ImGui::SameLine();
    ImGui::BeginChild("Details", {0, 0}, ImGuiChildFlags_Borders);
    details(catalog, textures, hidden);
    ImGui::EndChild(); ImGui::End();
}

void AssetBrowser::details(content::Catalog& catalog, TextureCache& textures, bool hidden)
{
    const auto* asset = catalog.find(selected_);
    if (!asset) { ImGui::TextWrapped("Select an asset. Double-click a folder or press Enter to open it. Use arrows/Home/End in the grid to navigate."); return; }
    if (hidden) ImGui::TextWrapped("Selection retained; hidden by the current folder or filters.");
    ImGui::TextWrapped("%s", asset->name.c_str());
    if (asset->type == content::AssetType::image) {
        if (const auto* image = textures.load(asset->key)) {
            const ImVec2 size{ImGui::GetContentRegionAvail().x, 200 * ImGui::GetStyle().FontScaleDpi};
            preview(*image, ImGui::GetCursorScreenPos(), size);
            ImGui::Dummy(size);
        }
    } else if (asset->type == content::AssetType::folder && ImGui::Button("Open folder")) folder_ = asset->key;
    ImGui::Separator();
    ImGui::TextWrapped("Path: %s", asset->key.c_str());
    ImGui::Text("Type: %s", content::type_name(asset->type));
    ImGui::Text("Size: %llu bytes", static_cast<unsigned long long>(asset->size));
    if (asset->dimensions.x) ImGui::Text("Dimensions: %d x %d", asset->dimensions.x, asset->dimensions.y);
    ImGui::Text("Loader: %s", asset->loader.empty() ? "None" : asset->loader.c_str());
    ImGui::Text("Status: %s", asset->type == content::AssetType::folder ? "Navigation" : content::status_name(asset->status));
    if (!asset->diagnostic.empty()) ImGui::TextWrapped("%s", asset->diagnostic.c_str());
    if (asset->type == content::AssetType::font) ImGui::TextWrapped("Font bytes are available to the GUI; font preview is not supported.");
}
