#include "entity_explorer.h"
#include "simulation/simulation.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace {
std::string text_value(const PropertyValue& value)
{
    return std::visit([](const auto& v) { std::ostringstream out; out << std::boolalpha << v; return out.str(); }, value);
}
std::string lower(std::string value)
{
    for (auto& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
bool contains(const std::string& value, const char* search)
{
    return lower(value).find(lower(search)) != std::string::npos;
}
bool widget(EntityProperty& field)
{
    bool changed = false;
    if (!field.choices.empty()) {
        const auto current = std::get<std::int64_t>(field.value);
        std::string preview = std::to_string(current);
        for (const auto& choice : field.choices) if (choice.value == current) preview = choice.name;
        if (ImGui::BeginCombo("##value", preview.c_str())) {
            for (const auto& choice : field.choices) {
                if (ImGui::Selectable(choice.name.c_str(), current == choice.value)) {
                    field.value = choice.value; changed = true;
                }
            }
            ImGui::EndCombo();
        }
    } else {
        std::visit([&](auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, bool>) changed = ImGui::Checkbox("##value", &value);
            else if constexpr (std::is_same_v<T, std::int64_t>) changed = ImGui::InputScalar("##value", ImGuiDataType_S64, &value);
            else if constexpr (std::is_same_v<T, std::uint64_t>) changed = ImGui::InputScalar("##value", ImGuiDataType_U64, &value);
            else if constexpr (std::is_same_v<T, double>) changed = ImGui::InputDouble("##value", &value, 0, 0, "%.4f");
            else {
                std::vector<char> buffer(value.size() + 256, 0);
                std::copy(value.begin(), value.end(), buffer.begin());
                if (ImGui::InputText("##value", buffer.data(), buffer.size())) { value = buffer.data(); changed = true; }
            }
        }, field.value);
    }
    return changed;
}
const EntityProperty* property(const EntityInspection& live, const EntityProperty& key)
{
    for (const auto& component : live.components)
        for (const auto& field : component.properties)
            if (field.component == key.component && field.path == key.path) return &field;
    return nullptr;
}
}

void EntityExplorer::open() { open_ = true; focus_ = true; next_refresh_ = 0; }
void EntityExplorer::select(EntityHandle handle)
{
    if (handle == selected_) { if (handle) open(); return; }
    if (dirty_) {
        requested_selection_ = handle;
        open();
        return;
    }
    switch_selection(handle);
}
void EntityExplorer::switch_selection(EntityHandle handle)
{
    requested_selection_.reset();
    selected_ = handle;
    captured_.reset(); staged_.clear(); dirty_ = false; ignite_ = false; message_.clear();
    if (handle) open();
}
void EntityExplorer::reload(const EntityInspection& live)
{
    captured_ = live;
    staged_.clear();
    for (const auto& component : live.components)
        for (const auto& field : component.properties) if (field.editable) staged_.push_back(field);
    dirty_ = false; ignite_ = false; burn_seconds_ = 16.0;
}

void EntityExplorer::draw(Simulation& simulation)
{
    auto& world = simulation.world();
    if (generation_ != world.generation()) {
        generation_ = world.generation();
        if (!world.entity_alive(selected_)) switch_selection({});
        scope_ = {}; pending_ = 0; next_refresh_ = 0; entities_.clear();
    }
    if (selected_ && !world.entity_alive(selected_)) switch_selection({});
    if (pending_) {
        if (auto result = world.take_edit_result(pending_)) {
            message_ = result->message;
            pending_ = 0;
            if (result->applied && selected_ == submitted_target_) {
                if (auto live = world.inspect_entity(selected_)) reload(*live);
                next_refresh_ = 0;
            }
        }
    }
    if (!open_) return;
    ImGui::SetNextWindowSize(ImVec2(960, 640), ImGuiCond_FirstUseEver);
    if (focus_) { ImGui::SetNextWindowFocus(); focus_ = false; }
    if (!ImGui::Begin("Entity Explorer", &open_)) { ImGui::End(); return; }
    if (requested_selection_) ImGui::OpenPopup("Discard staged changes?");
    if (ImGui::BeginPopupModal("Discard staged changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!requested_selection_) {
            ImGui::CloseCurrentPopup();
        } else {
            ImGui::TextUnformatted("This entity has staged changes.");
            if (ImGui::Button("Discard and select")) {
                switch_selection(*requested_selection_);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Keep editing")) { requested_selection_.reset(); ImGui::CloseCurrentPopup(); }
        }
        ImGui::EndPopup();
    }
    if (ImGui::Checkbox("All entities", &all_)) next_refresh_ = 0;
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) next_refresh_ = 0;
    ImGui::SameLine();
    ImGui::TextDisabled("Live list refreshes every 0.25 s");
    ImGui::InputTextWithHint("Search", "Name, path, Flecs or actor ID", search_, sizeof(search_));
    ImGui::InputTextWithHint("Component", "Component, tag or pair name", component_filter_, sizeof(component_filter_));
    ImGui::SetNextItemWidth(150);
    ImGui::Combo("Kind", &kind_, "Any\0Tree\0System\0Component\0Module\0Entity\0");
    ImGui::SameLine(); ImGui::SetNextItemWidth(150);
    ImGui::Combo("Sort", &sort_, "Full ID\0Name/path\0");
    if (scope_ && !world.entity_alive(scope_)) scope_ = {};
    if (ImGui::Button("Flat: all parents")) scope_ = {};
    if (scope_) {
        ImGui::SameLine();
        ImGui::Text("Children of #%llu", static_cast<unsigned long long>(scope_.entity));
        ImGui::SameLine();
        if (ImGui::Button("Up")) {
            if (auto parent = world.inspect_entity(scope_)) scope_ = parent->entity.parent;
        }
    }
    if (ImGui::GetTime() >= next_refresh_) {
        entities_ = world.entities(); next_refresh_ = ImGui::GetTime() + 0.25;
    }
    static const char* kinds[] = {"", "Tree", "System", "Component", "Module", "Entity"};
    std::vector<const EntitySummary*> filtered;
    for (const auto& entity : entities_) {
        if (!all_ && !entity.actor_id) continue;
        if (scope_ && entity.parent != scope_) continue;
        if (kind_ && entity.kind != kinds[kind_]) continue;
        const auto identity = entity.path + " " + std::to_string(entity.handle.entity) + " " +
            (entity.actor_id ? std::to_string(*entity.actor_id) : "");
        if (!contains(identity, search_)) continue;
        if (component_filter_[0] && std::none_of(entity.components.begin(), entity.components.end(),
                [&](const auto& name) { return contains(name, component_filter_); })) continue;
        filtered.push_back(&entity);
    }
    if (sort_) std::sort(filtered.begin(), filtered.end(), [](auto a, auto b) {
        return a->path == b->path ? a->handle.entity < b->handle.entity : a->path < b->path;
    });
    ImGui::Text("%zu results / %zu live entities", filtered.size(), entities_.size());
    ImGui::BeginChild("Entity list", ImVec2(360, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(filtered.size()));
    while (clipper.Step()) for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
        const auto& entity = *filtered[i];
        std::string label = entity.path + "  #" + std::to_string(entity.handle.entity) + "  " + entity.kind;
        if (entity.actor_id) label += " actor " + std::to_string(*entity.actor_id);
        if (entity.disabled) label += " [disabled]";
        if (entity.prefab) label += " [prefab]";
        ImGui::BeginDisabled(pending_ != 0);
        if (ImGui::Selectable(label.c_str(), selected_ == entity.handle)) select(entity.handle);
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("Inspector", ImVec2(0, 0));
    if (auto live = world.inspect_entity(selected_)) draw_details(simulation, *live);
    else ImGui::TextUnformatted("Select an entity to inspect components and properties.");
    ImGui::EndChild();
    ImGui::End();
}

void EntityExplorer::draw_details(Simulation& simulation, const EntityInspection& live)
{
    if (!captured_) reload(live);
    ImGui::SeparatorText(live.entity.path.c_str());
    ImGui::Text("Full ID: %llu   World: %llu", static_cast<unsigned long long>(selected_.entity),
                static_cast<unsigned long long>(selected_.generation));
    ImGui::BeginDisabled(pending_ != 0);
    if (live.entity.parent) {
        if (ImGui::Button("Select parent")) { select(live.entity.parent); ImGui::EndDisabled(); return; }
        ImGui::SameLine();
    }
    if (ImGui::Button("Browse children")) { scope_ = selected_; all_ = true; }
    ImGui::SameLine(); ImGui::TextDisabled("Flat list always includes all parents.");
    ImGui::BeginChild("Properties", ImVec2(0, -110));
    for (const auto& component : live.components) {
        ImGui::PushID(std::to_string(component.id).c_str());
        // Selection policy uses a distinct synthetic section for the Selectable tag.
        ImGui::PushID(component.name.c_str());
        if (ImGui::TreeNodeEx(component.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextDisabled("%s", component.detail.c_str());
            for (std::size_t i = 0; i < component.references.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                const auto target = component.references[i];
                if (ImGui::SmallButton(("Follow #" + std::to_string(target.entity)).c_str())) {
                    select(target); ImGui::PopID(); ImGui::TreePop(); ImGui::PopID(); ImGui::PopID();
                    ImGui::EndChild(); ImGui::EndDisabled(); return;
                }
                ImGui::SameLine(); ImGui::PopID();
            }
            if (!component.references.empty()) ImGui::NewLine();
            for (const auto& field : component.properties) {
                ImGui::PushID(field.path.c_str());
                ImGui::Text("%s: %s %s", field.path.c_str(), text_value(field.value).c_str(), field.units.c_str());
                if (field.editable) {
                    const auto staged = std::find_if(staged_.begin(), staged_.end(), [&](const auto& candidate) {
                        return candidate.component == field.component && candidate.path == field.path;
                    });
                    if (staged != staged_.end()) {
                        ImGui::SetNextItemWidth(220);
                        dirty_ |= widget(*staged);
                        ImGui::SameLine(); ImGui::TextDisabled("Staged");
                        if (field.minimum && field.maximum)
                            ImGui::TextDisabled("Range: %g to %g", *field.minimum, *field.maximum);
                    } else ImGui::TextDisabled("New live property; Revert to load its editor.");
                } else {
                    ImGui::TextDisabled("Read-only: %s", field.reason.c_str());
                }
                if (field.reference && ImGui::SmallButton("Follow value")) {
                    select(field.reference); ImGui::PopID(); ImGui::TreePop(); ImGui::PopID(); ImGui::PopID();
                    ImGui::EndChild(); ImGui::EndDisabled(); return;
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        ImGui::PopID(); ImGui::PopID();
    }
    if (live.tree && !live.tree->charred && !live.tree->burning()) {
        ImGui::SeparatorText("Fire command");
        if (ImGui::Checkbox("Ignite on Apply", &ignite_)) dirty_ = true;
        ImGui::SetNextItemWidth(180);
        ImGui::BeginDisabled(!ignite_);
        if (ImGui::InputDouble("Burn duration (s)", &burn_seconds_, 0.25, 1, "%.2f")) dirty_ = true;
        ImGui::EndDisabled();
        ImGui::TextDisabled("12–20 seconds, 0.25 s steps. Ignition and duration apply together.");
    }
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::TextUnformatted(dirty_ ? "Staged changes; live values continue updating above." : "No staged changes.");
    if (ImGui::Button("Apply")) {
        EntityEdit edit; edit.target = selected_;
        for (const auto& staged : staged_) {
            const auto* before = property(*captured_, staged);
            if (before && before->value != staged.value)
                edit.properties.push_back({staged.component, staged.path, before->value, staged.value});
        }
        if (ignite_ && captured_->tree) edit.ignite = IgniteEdit{*captured_->tree, burn_seconds_};
        submitted_target_ = selected_;
        pending_ = simulation.submit_edit(std::move(edit));
        message_ = pending_ ? "Pending simulation command boundary..." : "Command queue is full. Try again.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert / reload live")) { reload(live); message_ = "Reloaded live values."; }
    ImGui::EndDisabled();
    if (!message_.empty()) ImGui::TextWrapped("%s", message_.c_str());
}
