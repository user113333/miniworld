#include "island_controls.h"
#include "simulation/simulation.h"

#include <imgui.h>

#include <algorithm>

void draw_island_controls(Simulation& simulation, bool& open)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scale = ImGui::GetStyle().FontScaleDpi;
    const float margin = 12.0f * scale;
    // WorkPos can still describe the previous frame when the DPI first changes.
    const float top = glm::max(viewport->WorkPos.y, viewport->Pos.y + ImGui::GetFrameHeight());
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - margin, top + margin),
                            ImGuiCond_FirstUseEver, ImVec2(1, 0));
    ImGui::SetNextWindowSize(ImVec2(240.0f * scale, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Island", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Current seed: %u", simulation.map().seed);
        IslandSettings& settings = simulation.island_settings;
        ImGui::InputScalar("Seed", ImGuiDataType_U32, &settings.seed);
        ImGui::InputInt("Width", &settings.width);
        ImGui::InputInt("Height", &settings.height);
        settings.width = std::clamp(settings.width, 12, 96);
        settings.height = std::clamp(settings.height, 12, 72);
        float density_percent = settings.tree_density * 100.0f;
        if (ImGui::SliderFloat("Initial tree density", &density_percent, 0.0f, 100.0f, "%.0f%%")) {
            settings.tree_density = density_percent / 100.0f;
        }
        if (ImGui::Button("Generate")) {
            simulation.generate();
        }
        ImGui::SameLine();
        if (ImGui::Button("Regenerate")) {
            simulation.regenerate();
        }
        ImGui::TextUnformatted("Seed, size and density apply on generation.");
    }
    ImGui::End();
}
