#include "simulation_controls.h"
#include "simulation/simulation.h"

#include <imgui.h>

#include <algorithm>

void draw_simulation_controls(Simulation& simulation, EntityTool& tool, bool& open)
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
        SimulationSettings& settings = simulation.settings;
        int speed = static_cast<int>(settings.speed);
        if (ImGui::SliderInt("Speed", &speed, 0,
                             static_cast<int>(SimulationClock::max_speed), "%dx",
                             ImGuiSliderFlags_AlwaysClamp)) {
            settings.speed = static_cast<float>(speed);
        }
        if (ImGui::Button(settings.speed > 0.0f ? "Pause" : "Resume")) {
            simulation.toggle_pause();
        }
        ImGui::SameLine();
        ImGui::Text("Simulation: %.2f s", simulation.clock().elapsed_seconds());
        ImGui::Separator();
        const auto actors = simulation.world().actors();
        const auto trees = std::count_if(actors.begin(), actors.end(),
            [](const ActorView& actor) { return actor.tree.has_value(); });
        const auto dead = std::count_if(actors.begin(), actors.end(),
            [](const ActorView& actor) { return actor.tree && actor.tree->charred; });
        ImGui::Text("Trees: %zu", static_cast<std::size_t>(trees));
        const auto settlers = std::count_if(actors.begin(), actors.end(), [](const ActorView& actor) { return actor.settler.has_value(); });
        ImGui::Text("Settlers: %zu", static_cast<std::size_t>(settlers));
        ImGui::Text("Charred remains: %zu", static_cast<std::size_t>(dead));
        ImGui::Separator();
        bool select = tool == EntityTool::select;
        if (ImGui::Checkbox("Select entities", &select)) tool = select ? EntityTool::select : EntityTool::none;
        bool ignite = tool == EntityTool::ignite;
        if (ImGui::Checkbox("Ignite trees", &ignite)) tool = ignite ? EntityTool::ignite : EntityTool::none;
        bool spawn = tool == EntityTool::spawn_settler;
        if (ImGui::Checkbox("Spawn settler", &spawn)) tool = spawn ? EntityTool::spawn_settler : EntityTool::none;
        if (spawn) ImGui::TextUnformatted("Click walkable land. Esc exits the tool.");
        bool camera = tool == EntityTool::camera;
        if (ImGui::Checkbox("Camera control (Space)", &camera)) {
            tool = camera ? EntityTool::camera : EntityTool::none;
        }
        if (tool == EntityTool::camera) {
            ImGui::TextUnformatted("Drag to pan. Scroll to zoom. Esc exits.");
        }
        if (tool == EntityTool::select) {
            ImGui::TextUnformatted("Click a tree or settler to inspect it. Esc exits the tool.");
        }
        if (tool == EntityTool::ignite) {
            ImGui::TextUnformatted("Click a tree. Esc exits the tool.");
        }
        if (simulation.pending_ignitions() > 0) {
            ImGui::Text("Ignitions queued: %zu%s", simulation.pending_ignitions(),
                        settings.speed == 0.0f ? " (paused)" : "");
        }
    }
    ImGui::End();
}
