#include "tree_inspector.h"
#include "island_demo.h"

#include <imgui.h>

#include <algorithm>

namespace {

const char* stage_name(TreeStage stage)
{
    switch (stage) {
    case TreeStage::growing: return "Growing";
    case TreeStage::mature: return "Mature";
    case TreeStage::dead: return "Dead";
    }
    return "Unknown";
}

} // namespace

void TreeInspector::select(std::uint64_t tree_id, std::uint64_t generation)
{
    tree_id_ = tree_id;
    generation_ = generation;
    focus_requested_ = tree_id != 0;
}

void TreeInspector::draw(const IslandDemo& island)
{
    if (tree_id_ == 0) {
        return;
    }
    const auto& trees = island.map().trees;
    const auto tree = std::find_if(trees.begin(), trees.end(),
        [this](const Tree& candidate) { return candidate.id == tree_id_; });
    if (generation_ != island.world_generation() || tree == trees.end()) {
        tree_id_ = 0;
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scale = ImGui::GetStyle().FontScaleDpi;
    const float margin = 12.0f * scale;
    const float top = glm::max(viewport->WorkPos.y, viewport->Pos.y + ImGui::GetFrameHeight());
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + margin, top + margin),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240.0f * scale, 0), ImGuiCond_FirstUseEver);
    if (focus_requested_) {
        ImGui::SetNextWindowFocus();
        ImGui::SetNextWindowCollapsed(false);
        focus_requested_ = false;
    }
    bool open = true;
    if (ImGui::Begin("Tree", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("ID: %llu", static_cast<unsigned long long>(tree->id));
        ImGui::Text("Hex: %d, %d", tree->hex.x, tree->hex.y);
        ImGui::Text("Stage: %s", stage_name(tree->stage));
        ImGui::Text("Age: %.1f s", tree->age_ticks * SimulationClock::base_step_seconds);
        ImGui::Text("Fire: %s", tree->burning() ? "Burning" : tree->charred ? "Charred" : "None");
        if (tree->burning()) {
            const auto ticks = tree->burnout_tick > island.clock().ticks()
                ? tree->burnout_tick - island.clock().ticks() : 0;
            ImGui::Text("Burns out in: %.1f s", ticks * SimulationClock::base_step_seconds);
        }
    }
    ImGui::End();
    if (!open) {
        tree_id_ = 0;
    }
}
