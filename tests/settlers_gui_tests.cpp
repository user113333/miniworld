#include "render/map_renderer.h"
#include "gui/application_ui.h"
#include "simulation/simulation.h"
#include "world/hex_grid.h"
#include "test_support.h"
#include <SDL3/SDL.h>
#include <imgui_internal.h>
#include <cfloat>

namespace {
class Gui {
public:
    Gui() {
        test::expect(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
        window = SDL_CreateWindow("Settlers review", 1440, 960, SDL_WINDOW_HIDDEN);
        renderer = SDL_CreateRenderer(window, "software");
        test::expect(window && renderer, SDL_GetError());
        test::expect(content.initialize(std::filesystem::path(SDL_GetBasePath()) / "res"), content.error());
        textures.initialize(renderer, content);
        test::expect(ui.initialize(window, renderer, content, textures), "cannot initialize settlers GUI");
        ImGui::GetIO().IniFilename = nullptr;
        simulation.island_settings.seed = 74129; simulation.regenerate(); simulation.settings.speed = 0;
    }
    ~Gui() { ui.shutdown(); textures.shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); }
    void frame() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) ui.process_event(event);
        ui.begin_frame(window);
        bool running = true; ui.draw(running, simulation); ui.finish_frame();
        SDL_SetRenderDrawColor(renderer, 24, 28, 32, 255); SDL_RenderClear(renderer); ui.render(renderer); SDL_RenderPresent(renderer);
        simulation.advance_simulation(0);
    }
    void activate(ImGuiWindow* owner, ImGuiID id) {
        ImGui::FocusWindow(owner); ImGui::ActivateItemByID(id); frame(); frame();
    }
    void button(ImGuiWindow* owner, const char* label) { activate(owner, owner->GetID(label)); }
    void key(ImGuiKey key) {
        ImGui::GetIO().AddKeyEvent(key, true); frame(); ImGui::GetIO().AddKeyEvent(key, false); frame();
    }
    void text(ImGuiWindow* owner, ImGuiID id, const char* value) {
        ImGui::FocusWindow(owner); ImGui::ActivateItemByID(id); GImGui->NavNextActivateFlags = ImGuiActivateFlags_PreferInput;
        frame(); frame();
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true); key(ImGuiKey_A);
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false); key(ImGuiKey_Backspace);
        ImGui::GetIO().AddInputCharactersUTF8(value); frame(); frame(); key(ImGuiKey_Enter);
    }
    void free_mouse() { ImGui::GetIO().AddMousePosEvent(-FLT_MAX, -FLT_MAX); frame(); frame(); }
    SDL_Window* window = nullptr; SDL_Renderer* renderer = nullptr;
    content::Catalog content; TextureCache textures; ApplicationUi ui; Simulation simulation;
};
ImGuiID slot_id(ImGuiWindow* window, int slot, const char* label)
{
    auto id = ImHashStr("Inventory", 0, window->ID);
    id = ImHashData(&slot, sizeof(slot), id);
    return ImHashStr(label, 0, id);
}
void check_gui()
{
    Gui gui;
    ActorHandle target, other;
    for (const auto& actor : gui.simulation.world().actors()) {
        if (actor.settler && !target) target = actor.handle;
        if (actor.tree) other = actor.handle;
    }
    gui.ui.select_actor(target); gui.frame(); gui.frame();
    auto* settlers = ImGui::FindWindowByName("Settlers");
    auto* explorer = ImGui::FindWindowByName("Entity Explorer");
    test::expect(settlers && settlers->Active && explorer && explorer->Active, "settler selection did not open shared inspectors");
    ImGui::SetWindowPos(settlers, {1000, 60}); ImGui::SetWindowSize(settlers, {420, 850});
    gui.frame(); gui.frame();
    gui.text(explorer, explorer->GetID("Search"), "Settler");
    // All three settlers are searchable in the production list, not just a bespoke window.
    ImGuiWindow* list = nullptr;
    for (auto* candidate : GImGui->Windows)
        if (candidate->ParentWindow == explorer && std::string(candidate->Name).find("/Entity list_") != std::string::npos) list = candidate;
    test::expect(list && list->ScrollMax.y == 0, "settler search did not narrow the entity list");
    gui.activate(settlers, slot_id(settlers, 0, "##item"));
    gui.key(ImGuiKey_DownArrow); gui.key(ImGuiKey_Enter);
    gui.text(settlers, slot_id(settlers, 0, "##quantity"), "7");
    const auto state = [&] { return gui.simulation.world().inspect(target)->settler.value(); };
    test::expect(state().inventory.count(ItemType::seeds) == 0, "staging changed Flecs before Apply");
    gui.ui.select_actor(other); gui.frame(); gui.frame();
    auto* discard = ImGui::FindWindowByName("Discard staged changes?");
    test::expect(discard && discard->Active && gui.ui.selected_actor() == target, "selection silently discarded inventory draft");
    gui.button(discard, "Keep editing");
    gui.button(settlers, "Apply inventory");
    test::expect(state().inventory.count(ItemType::seeds) == 7, "GUI inventory editor did not apply seeds");
    test::expect(gui.simulation.clock().ticks() == 0, "paused inventory editing advanced time");
    gui.text(settlers, slot_id(settlers, 0, "##quantity"), "9");
    auto live = state(); live.inventory.consume(ItemType::seeds);
    gui.simulation.world().registry().entity(target.entity).set<Settler>(live);
    gui.button(settlers, "Apply inventory");
    test::expect(state().inventory.count(ItemType::seeds) == 6, "GUI overwrote concurrent inventory change");
    gui.button(settlers, "Revert inventory");
    gui.text(settlers, slot_id(settlers, 0, "##quantity"), "99");
    gui.button(settlers, "Apply inventory");
    test::expect(state().inventory.count(ItemType::seeds) == 10, "quantity UI did not clamp to ten");
    gui.text(settlers, slot_id(settlers, 0, "##quantity"), "0");
    gui.button(settlers, "Apply inventory");
    test::expect(state().inventory.slots[0].type == ItemType::empty, "GUI did not clear zero quantity slot");
    // Last slot is an independently addressable real widget.
    gui.activate(settlers, slot_id(settlers, 7, "##item")); gui.key(ImGuiKey_DownArrow); gui.key(ImGuiKey_DownArrow); gui.key(ImGuiKey_Enter);
    gui.text(settlers, slot_id(settlers, 7, "##quantity"), "4"); gui.button(settlers, "Apply inventory");
    test::expect(state().inventory.slots[7].type == ItemType::logs && state().inventory.slots[7].quantity == 4, "eighth inventory slot not editable");
    gui.button(settlers, "Spawn settler");
    test::expect(gui.ui.tool() == EntityTool::spawn_settler, "spawn tool did not activate");
    const auto count = gui.simulation.world().actors().size();
    const auto land = gui.simulation.world().inspect(target)->position.hex;
    const MapView view{{700, 450}, 20};
    const auto click = view.origin + (hex_center(land) + glm::vec2{.2f, .1f}) * view.radius;
    // Production capture state suppresses clicks even with an active spawn tool.
    ImGui::GetIO().AddMousePosEvent(settlers->Pos.x + 100, settlers->Pos.y + 80); gui.frame(); gui.frame();
    test::expect(gui.ui.frame_state().capture_mouse, "settlers window does not capture mouse");
    gui.ui.spawn_settler_at(gui.simulation, view, click, gui.simulation.world_generation()); gui.frame();
    test::expect(gui.simulation.world().actors().size() == count, "captured click spawned a settler");
    gui.free_mouse();
    gui.ui.spawn_settler_at(gui.simulation, view, click, gui.simulation.world_generation());
    test::expect(gui.simulation.world().actors().size() == count, "click spawned before command boundary");
    gui.frame(); gui.frame();
    test::expect(gui.simulation.world().actors().size() == count + 1, "world click did not spawn settler");
    const auto spawned = gui.simulation.world().inspect(gui.ui.selected_actor());
    test::expect(spawned && spawned->settler && spawned->handle != target && glm::length(spawned->position.offset - glm::vec2(.2f, .1f)) < .0001f,
                 "spawn did not select a new Flecs settler at the click position");
    glm::ivec2 water{};
    for (int i = 0; i < static_cast<int>(gui.simulation.map().tiles.size()); ++i)
        if (gui.simulation.map().tiles[i] == Terrain::water) { water = gui.simulation.map().hex_at(i); break; }
    gui.ui.spawn_settler_at(gui.simulation, view, view.origin + hex_center(water) * view.radius, gui.simulation.world_generation()); gui.frame(); gui.frame();
    test::expect(gui.simulation.world().actors().size() == count + 1, "water click spawned settler");
    gui.ui.cancel_tool();
    gui.ui.spawn_settler_at(gui.simulation, view, click, gui.simulation.world_generation()); gui.frame();
    test::expect(gui.simulation.world().actors().size() == count + 1, "cancelled tool accepts clicks");
    gui.simulation.world().destroy(gui.ui.selected_actor()); gui.frame(); gui.frame();
    test::expect(!gui.ui.selected_actor(), "deleted settler selection survives");
    gui.ui.select_actor(target); gui.frame();
    gui.text(settlers, slot_id(settlers, 7, "##quantity"), "5");
    gui.ui.select_actor(other); gui.frame(); gui.frame();
    discard = ImGui::FindWindowByName("Discard staged changes?");
    test::expect(discard && discard->Active, "second draft did not protect selection");
    gui.button(discard, "Discard and select");
    test::expect(gui.ui.selected_actor() == other, "discard did not switch shared selection");
    gui.ui.select_actor(target); gui.frame(); gui.frame();
    gui.simulation.regenerate(); gui.frame(); gui.frame();
    test::expect(!gui.ui.selected_actor() && gui.ui.tool() == EntityTool::none, "regeneration retained selection/tool");
    gui.ui.select_actor(gui.simulation.world().spawn_settler({})); gui.frame(); gui.frame();
    SDL_Surface* image = SDL_RenderReadPixels(gui.renderer, nullptr);
    test::expect(image && SDL_SavePNG(image, "/tmp/miniworld-settlers-ui.png"), "cannot capture settlers window"); SDL_DestroySurface(image);
}
}
int main() { return test::run("Settlers GUI", check_gui); }
