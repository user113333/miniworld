#include "simulation_controls.h"
#include "island_demo.h"

#include <imgui.h>

#include <algorithm>

void draw_simulation_controls(IslandDemo& island, bool& open)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scale = ImGui::GetStyle().FontScaleDpi;
    const float margin = 12.0f * scale;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - margin,
                                 viewport->WorkPos.y + viewport->WorkSize.y - margin),
                            ImGuiCond_FirstUseEver, ImVec2(1, 1));
    // Leave room for tool hints and queued commands when anchoring at the bottom right.
    ImGui::SetNextWindowSize(ImVec2(320.0f * scale, 240.0f * scale), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Simulation", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        IslandSettings& settings = island.settings;
        int speed = static_cast<int>(settings.simulation_speed);
        if (ImGui::SliderInt("Speed", &speed, 0,
                             static_cast<int>(SimulationClock::max_speed), "%dx",
                             ImGuiSliderFlags_AlwaysClamp)) {
            settings.simulation_speed = static_cast<float>(speed);
        }
        if (ImGui::Button(settings.simulation_speed > 0.0f ? "Pause" : "Resume")) {
            island.toggle_pause();
        }
        ImGui::SameLine();
        ImGui::Text("Simulation: %.2f s", island.clock().elapsed_seconds());
        ImGui::Separator();
        ImGui::Text("Trees: %zu", island.map().trees.size());
        const auto growing = std::count_if(island.map().trees.begin(), island.map().trees.end(),
            [](const Tree& tree) { return tree.stage == TreeStage::growing; });
        const auto dead = std::count_if(island.map().trees.begin(), island.map().trees.end(),
            [](const Tree& tree) { return tree.stage == TreeStage::dead; });
        ImGui::Text("Saplings: %zu  Dead: %zu", static_cast<std::size_t>(growing),
                    static_cast<std::size_t>(dead));
        ImGui::Separator();
        if (ImGui::Checkbox("Select entities", &settings.select_entities) && settings.select_entities) {
            settings.ignite_trees = false;
        }
        if (ImGui::Checkbox("Ignite trees", &settings.ignite_trees) && settings.ignite_trees) {
            settings.select_entities = false;
        }
        if (settings.select_entities) {
            ImGui::TextUnformatted("Click a tree to inspect it. Esc exits the tool.");
        }
        if (settings.ignite_trees) {
            ImGui::TextUnformatted("Click a tree. Esc exits the tool.");
        }
        if (island.pending_ignitions() > 0) {
            ImGui::Text("Ignitions queued: %zu%s", island.pending_ignitions(),
                        settings.simulation_speed == 0.0f ? " (paused)" : "");
        }
    }
    ImGui::End();
}
