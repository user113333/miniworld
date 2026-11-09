#include "actor_inspector.h"
#include "simulation/simulation.h"

#include <imgui.h>

#include <algorithm>

void ActorInspector::select(ActorHandle actor)
{
    actor_ = actor;
    focus_requested_ = static_cast<bool>(actor);
}

void ActorInspector::draw(const Simulation& simulation)
{
    if (!actor_) return;
    const auto actor = simulation.world().inspect(actor_);
    if (!actor) {
        actor_ = {};
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
    const char* title = actor->tree ? "Tree" : "Entity";
    if (ImGui::Begin(title, &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("ID: %llu", static_cast<unsigned long long>(actor->id));
        ImGui::Text("Hex: %d, %d", actor->position.hex.x, actor->position.hex.y);
        if (actor->tree) {
            const Tree& tree = *actor->tree;
            ImGui::Text("Fire: %s", tree.burning() ? "Burning" : tree.charred ? "Charred" : "None");
            if (tree.burning()) {
                const auto ticks = tree.burnout_tick > simulation.clock().ticks()
                    ? tree.burnout_tick - simulation.clock().ticks() : 0;
                ImGui::Text("Burns out in: %.1f s", ticks * SimulationClock::base_step_seconds);
            }
        }
    }
    ImGui::End();
    if (!open) {
        actor_ = {};
    }
}
