#include "settlers_window.h"
#include "entity_explorer.h"
#include "simulation/simulation.h"
#include <imgui.h>
#include <algorithm>

void SettlersWindow::reload(const Settler& settler)
{
    captured_ = staged_ = settler.inventory;
    dirty_ = false;
}
void SettlersWindow::spawn(Simulation& simulation, ActorPosition position, std::uint64_t generation)
{
    open();
    const auto request = simulation.world().submit_spawn({position, generation});
    if (request) { pending_spawns_.push_back(request); message_ = "Spawn queued..."; }
    else message_ = "Command queue is full. Try again.";
}
void SettlersWindow::draw(Simulation& simulation, EntityExplorer& explorer, EntityTool& tool)
{
    auto& world = simulation.world();
    if (generation_ != world.generation()) {
        generation_ = world.generation(); selected_ = {}; pending_ = 0;
        pending_spawns_.clear(); dirty_ = false; message_.clear();
    }
    for (auto it = pending_spawns_.begin(); it != pending_spawns_.end();) {
        if (auto result = world.take_edit_result(*it)) {
            message_ = result->message;
            if (result->applied) explorer.select(result->spawned);
            it = pending_spawns_.erase(it);
        } else ++it;
    }
    if (pending_) {
        if (auto result = world.take_edit_result(pending_)) {
            message_ = result->message; pending_ = 0;
            if (result->applied && selected_ == submitted_target_)
                if (const auto actor = world.inspect(selected_); actor && actor->settler) reload(*actor->settler);
        }
    }
    const auto selection = explorer.selected();
    if (selection != selected_) {
        selected_ = selection; dirty_ = false;
        if (const auto actor = world.inspect(selected_); actor && actor->settler) { reload(*actor->settler); open(); }
    }
    const auto actor = world.inspect(selected_);
    if (!actor || !actor->settler) dirty_ = false;
    explorer.protect_selection(dirty_);
    if (!open_) return;
    const float scale = ImGui::GetStyle().FontScaleDpi;
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - 12 * scale,
                           viewport->WorkPos.y + 36 * scale}, ImGuiCond_FirstUseEver, {1, 0});
    ImGui::SetNextWindowSize({410 * scale, 650 * scale}, ImGuiCond_FirstUseEver);
    if (focus_) { ImGui::SetNextWindowFocus(); focus_ = false; }
    if (ImGui::Begin("Settlers", &open_)) {
        bool spawning = tool == EntityTool::spawn_settler;
        if (ImGui::Checkbox("Spawn settler", &spawning)) tool = spawning ? EntityTool::spawn_settler : EntityTool::none;
        ImGui::TextWrapped("Activate the tool, then click walkable land. Esc exits.");
        ImGui::Separator();
        if (actor && actor->settler) {
            const auto& settler = *actor->settler;
            ImGui::Text("Settler %llu (Flecs #%llu)", static_cast<unsigned long long>(actor->id),
                        static_cast<unsigned long long>(selected_.entity));
            ImGui::Text("Happiness: %d / 1000", settler.happiness);
            ImGui::ProgressBar(settler.happiness / 1000.0f, {-1, 0});
            ImGui::Text("Action: %s", settler_action_name(settler.action));
            ImGui::TextWrapped("Thought: %s", settler.thought_ticks ? settler.thought.c_str() : "None");
            if (settler.thought_ticks) ImGui::TextDisabled("Visible for %.1f s", settler.thought_ticks / 32.0);
            if (ImGui::CollapsingHeader("DNA", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("Movement speed: %.2fx", settler.dna.speed);
                ImGui::Text("People sensitivity: %.2f   Trees: %.2f", settler.dna.people_sensitivity, settler.dna.tree_sensitivity);
                ImGui::Text("Preferences: rest %.2f, wander %.2f, cut %.2f, plant %.2f",
                    settler.dna.preferences[0], settler.dna.preferences[1], settler.dna.preferences[2], settler.dna.preferences[3]);
            }
            ImGui::SeparatorText("Inventory (8 slots)");
            if (!dirty_ && !pending_) reload(settler);
            ImGui::BeginDisabled(pending_ != 0);
            if (ImGui::BeginTable("Inventory", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 40 * scale);
                ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch, 1.5f);
                ImGui::TableSetupColumn("Units", ImGuiTableColumnFlags_WidthStretch, 1); ImGui::TableHeadersRow();
                for (int i = 0; i < 8; ++i) {
                    auto& slot = staged_.slots[i];
                    ImGui::PushID(i);
                    ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("%d", i + 1);
                    ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
                    int type = static_cast<int>(slot.type);
                    if (ImGui::Combo("##item", &type, "Empty\0Seeds\0Logs\0")) {
                        slot.type = static_cast<ItemType>(type);
                        if (slot.type == ItemType::empty) slot.quantity = 0;
                        dirty_ = true;
                    }
                    ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
                    ImGui::BeginDisabled(slot.type == ItemType::empty);
                    if (ImGui::InputInt("##quantity", &slot.quantity, 0, 0)) {
                        slot.quantity = std::clamp(slot.quantity, 0, 10); dirty_ = true;
                    }
                    ImGui::EndDisabled(); ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::TextUnformatted(dirty_ ? "Staged changes; Apply to update inventory." : "Inventory is live.");
            ImGui::BeginDisabled(!dirty_);
            if (ImGui::Button("Apply inventory")) {
                EntityEdit edit; edit.target = selected_; edit.inventory = InventoryEdit{captured_, staged_};
                submitted_target_ = selected_; pending_ = simulation.submit_edit(std::move(edit));
                message_ = pending_ ? "Inventory queued..." : "Command queue is full. Try again.";
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Revert inventory")) { reload(settler); message_ = "Reloaded live inventory."; }
            ImGui::EndDisabled();
        } else ImGui::TextWrapped("Select a settler in the scene or Entity Explorer to inspect it.");
        if (!message_.empty()) ImGui::TextWrapped("%s", message_.c_str());
    }
    ImGui::End();
    explorer.protect_selection(dirty_);
}
