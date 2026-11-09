#include "gui/application_ui.h"
#include "simulation/simulation.h"
#include "test_support.h"
#include <SDL3/SDL.h>
#include <imgui_internal.h>
#include <cstdlib>

namespace {
// Drive the real application GUI; internal IDs only activate real widgets.
class Gui {
public:
    Gui()
    {
        test::expect(SDL_Init(SDL_INIT_VIDEO), "Cannot initialize SDL");
        window = SDL_CreateWindow("Explorer review", 1280, 960, SDL_WINDOW_HIDDEN);
        test::expect(window != nullptr, "Cannot create review window");
        renderer = SDL_CreateRenderer(window, "software");
        test::expect(content.initialize(std::filesystem::path(SDL_GetBasePath()) / "res"), content.error());
        textures.initialize(renderer, content);
        test::expect(renderer && ui.initialize(window, renderer, content, textures), "Cannot initialize production GUI");
        ImGui::GetIO().IniFilename = nullptr;
        simulation.island_settings.seed = 74129;
        simulation.regenerate();
        simulation.settings.speed = 0;
    }
    ~Gui()
    {
        ui.shutdown(); textures.shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    }
    void frame()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event)) ui.process_event(event);
        ui.begin_frame(window);
        bool running = true; ui.draw(running, simulation);
        ui.finish_frame();
        SDL_SetRenderDrawColor(renderer, 24, 28, 32, 255); SDL_RenderClear(renderer);
        ui.render(renderer);
        if (const char* path = std::getenv("EXPLORER_SCREENSHOT")) {
            SDL_Surface* image = SDL_RenderReadPixels(renderer, nullptr);
            test::expect(image && SDL_SaveBMP(image, path), "Cannot capture explorer review image");
            SDL_DestroySurface(image);
        }
        SDL_RenderPresent(renderer);
        simulation.advance_simulation(0);
    }
    void activate(ImGuiWindow* window, const char* label)
    {
        ImGui::ActivateItemByID(window->GetID(label));
        frame(); frame();
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    content::Catalog content;
    TextureCache textures;
    ApplicationUi ui;
    Simulation simulation;
};

void check_gui()
{
    Gui gui;
    const auto target = gui.simulation.world().actors().front().handle;
    gui.ui.select_actor(target);
    gui.frame(); gui.frame();
    auto* explorer = ImGui::FindWindowByName("Entity Explorer");
    test::expect(explorer && explorer->Active, "Scene selection did not open the explorer");
    ImGuiWindow* inspector = nullptr;
    // Child names carry a hash suffix; find the actual window by its parent.
    for (auto* candidate : GImGui->Windows)
        if (candidate->ParentWindow == explorer && candidate->Name && std::string(candidate->Name).find("/Inspector_") != std::string::npos)
            inspector = candidate;
    test::expect(inspector != nullptr, "Missing inspector pane");
    gui.activate(explorer, "All entities");
    // Thousands of unrelated entities exercise list clipping on the full registry.
    for (int i = 0; i < 5000; ++i) gui.simulation.world().registry().entity();
    gui.activate(explorer, "Refresh");
    test::expect(ImGui::GetDrawData()->TotalVtxCount < 100000, "Large entity list is not clipped");
    ImGui::GetIO().AddMousePosEvent(explorer->Pos.x + 100, explorer->Pos.y + 100);
    gui.frame(); gui.frame();
    test::expect(gui.ui.frame_state().capture_mouse, "Explorer mouse input is not captured from world tools");
    ImGuiWindow* properties = nullptr;
    for (auto* candidate : GImGui->Windows)
        if (candidate->ParentWindow == inspector && std::string(candidate->Name).find("/Properties_") != std::string::npos)
            properties = candidate;
    test::expect(properties != nullptr, "Missing property pane");
    const auto scroll_bottom = [&] {
        ImGui::SetScrollY(properties, properties->ScrollMax.y); gui.frame(); gui.frame();
    };
    const auto toggle_selection = [&] {
        scroll_bottom();
        ImGuiID id = properties->ID;
        for (const auto& part : {std::to_string(gui.simulation.world().registry().id<Selectable>()),
                                std::string("Selection policy"), std::string("Selection policy"),
                                std::string("selectable"), std::string("##value")})
            id = ImHashStr(part.c_str(), 0, id);
        ImGui::ActivateItemByID(id); gui.frame(); gui.frame();
    };
    toggle_selection(); // Stage false, then request a different selection.
    const auto other = gui.simulation.world().actors().back().handle;
    gui.ui.select_actor(other); gui.frame(); gui.frame();
    auto* discard = ImGui::FindWindowByName("Discard staged changes?");
    test::expect(discard && discard->Active, "Selection silently discarded the staged form");
    gui.activate(discard, "Keep editing");
    // A live change must conflict with the captured field, preserving the form.
    gui.simulation.world().registry().entity(target.entity).remove<Selectable>();
    gui.activate(inspector, "Apply");
    test::expect(!gui.simulation.world().inspect(target)->selectable, "Conflicting form changed live selection");
    gui.activate(inspector, "Revert / reload live");
    toggle_selection();
    gui.activate(inspector, "Apply");
    test::expect(gui.simulation.world().inspect(target)->selectable, "GUI Apply/Revert did not edit the selection tag");
    scroll_bottom();
    gui.activate(properties, "Ignite on Apply");
    gui.activate(inspector, "Apply");
    test::expect(gui.simulation.world().inspect(target)->tree->burning(), "GUI coupled fire editor did not ignite the tree");
    test::expect(gui.simulation.clock().ticks() == 0, "GUI browsing/apply advanced paused simulation time");
    toggle_selection();
    gui.ui.select_actor(other); gui.frame();
    gui.simulation.regenerate(); gui.frame(); gui.frame();
    test::expect(gui.simulation.world().inspect_entity(target) == std::nullopt && explorer->Active,
                 "Regeneration did not invalidate selection or keep the explorer usable");
    gui.ui.select_actor(gui.simulation.world().actors().front().handle);
    gui.frame(); gui.frame();
}
}
int main() { return test::run("Entity explorer GUI", check_gui); }
