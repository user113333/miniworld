#include "gui/application_ui.h"
#include "render/actor_renderer.h"
#include "simulation/simulation.h"
#include "content_test_support.h"
#include "test_support.h"
#include <SDL3/SDL.h>
#include <imgui_internal.h>
#include <cstdlib>

namespace {
void check_font_failures(SDL_Window* window, SDL_Renderer* renderer)
{
    for (bool corrupt : {false, true}) {
        AssetTestDirectory temp;
        if (corrupt) temp.write("fonts/CozetteVector.ttf", "invalid font bytes");
        content::Catalog content; test::expect(content.initialize(temp.path), content.error());
        TextureCache textures; textures.initialize(renderer, content);
        ApplicationUi ui;
        test::expect(!ui.initialize(window, renderer, content, textures), "Missing/corrupt required font did not fail startup");
        ui.shutdown(); textures.shutdown();
        test::expect(ImGui::GetCurrentContext() == nullptr, "Partial startup leaked the GUI context");
    }
}
class Gui {
public:
    explicit Gui(const std::filesystem::path& root)
    {
        test::expect(SDL_Init(SDL_INIT_VIDEO), "Cannot initialize SDL");
        window = SDL_CreateWindow("Assets review", 1280, 960, SDL_WINDOW_HIDDEN);
        test::expect(window != nullptr, "Cannot create assets window");
        renderer = SDL_CreateRenderer(window, "software");
        test::expect(renderer != nullptr, "Cannot create assets renderer");
        check_font_failures(window, renderer);
        test::expect(content.initialize(root), content.error());
        textures.initialize(renderer, content);
        test::expect(actors.initialize(textures), "Required tree assets failed");
        test::expect(ui.initialize(window, renderer, content, textures), "Cannot initialize assets GUI");
        ImGui::GetIO().IniFilename = nullptr;
        simulation.island_settings.seed = 74129; simulation.regenerate(); simulation.settings.speed = 0;
    }
    ~Gui()
    {
        ui.shutdown(); actors.shutdown(); textures.shutdown();
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    }
    void frame()
    {
        SDL_Event event; while (SDL_PollEvent(&event)) ui.process_event(event);
        ui.begin_frame(window); bool running = true; ui.draw(running, simulation); ui.finish_frame();
        SDL_SetRenderDrawColor(renderer, 24, 28, 32, 255); SDL_RenderClear(renderer);
        ui.render(renderer); SDL_RenderPresent(renderer);
        simulation.advance_simulation(0);
    }
    void activate(ImGuiWindow* window, const char* label)
    {
        ImGuiID seed = window->ID;
        if (std::string(window->Name) == "##MainMenuBar") seed = ImHashStr("##MenuBar", 0, seed);
        ImGui::ActivateItemByID(ImHashStr(label, 0, seed)); frame(); frame();
    }
    void item(ImGuiWindow* grid, const char* key)
    {
        ImGui::FocusWindow(grid);
        ImGui::ActivateItemByID(ImHashStr("##asset", 0, ImHashStr(key, 0, grid->ID)));
        frame(); frame();
    }
    void key(ImGuiKey key)
    { ImGui::GetIO().AddKeyEvent(key, true); frame(); ImGui::GetIO().AddKeyEvent(key, false); frame(); }
    void text(ImGuiWindow* window, const char* label, const char* value, bool scalar = false)
    {
        ImGui::FocusWindow(window);
        ImGui::ActivateItemByID(window->GetID(label));
        if (scalar) GImGui->NavNextActivateFlags = ImGuiActivateFlags_PreferInput;
        frame(); frame();
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true); key(ImGuiKey_A);
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false); key(ImGuiKey_Backspace);
        ImGui::GetIO().AddInputCharactersUTF8(value); frame(); frame();
    }
    void capture(const char* name)
    {
        if (const char* prefix = std::getenv("ASSET_SCREENSHOT")) {
            const auto path = std::string(prefix) + "-" + name + ".bmp";
            SDL_Surface* image = SDL_RenderReadPixels(renderer, nullptr);
            test::expect(image && SDL_SaveBMP(image, path.c_str()), "Cannot capture asset browser");
            SDL_DestroySurface(image);
        }
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    content::Catalog content;
    TextureCache textures;
    ActorRenderer actors;
    ApplicationUi ui;
    Simulation simulation;
};
ImGuiWindow* child(ImGuiWindow* parent, const std::string& name)
{
    for (auto* window : GImGui->Windows)
        if (window->ParentWindow == parent && std::string(window->Name).find("/" + name + "_") != std::string::npos) return window;
    throw std::runtime_error("Missing child " + name);
}
void check_browser()
{
    AssetTestDirectory temp; temp.copy_resources(std::filesystem::path(SDL_GetBasePath()) / "res");
    temp.write("broken.png", "bad optional PNG");
    for (int i = 0; i < 1000; ++i) temp.write("many/entry-" + std::to_string(i) + ".txt", "metadata only");
    Gui gui(temp.path);
    const auto before = gui.simulation.world().actors();
    const auto* tree = gui.textures.load("map/tree1.png");
    test::expect(gui.textures.loaded_count() == 6, "World did not load its five tree bindings and settler animation");
    gui.frame(); gui.frame();
    auto* bar = ImGui::FindWindowByName("##MainMenuBar");
    test::expect(bar != nullptr, "Missing main menu bar");
    gui.activate(bar, "Assets");
    auto* assets = ImGui::FindWindowByName("Assets");
    test::expect(assets && assets->Active, "Assets button did not open browser");
    auto* grid = child(assets, "Grid");
    gui.item(grid, "broken.png");
    test::expect(gui.content.find("broken.png")->status == content::LoadStatus::failed && assets->Active,
                 "Optional corrupt preview closed the browser or has no error");
    gui.capture("error");
    gui.item(grid, "map"); gui.key(ImGuiKey_Enter);
    test::expect(gui.textures.load("map/tree1.png") == tree && gui.textures.loaded_count() == 8 &&
                 gui.content.find("map/ship.png")->status == content::LoadStatus::loaded &&
                 gui.content.find("map/house.png")->status == content::LoadStatus::loaded,
                 "Map navigation failed or browser duplicated gameplay textures");
    gui.item(grid, "map/ship.png"); gui.capture("images");
    gui.key(ImGuiKey_LeftArrow); gui.key(ImGuiKey_RightArrow);
    gui.text(assets, "Search", "aseprite");
    test::expect(ImGui::GetIO().WantTextInput, "Asset search did not capture keyboard input");
    gui.key(ImGuiKey_Space);
    test::expect(gui.ui.tool() == EntityTool::none, "Typing activated a world tool");
    gui.capture("source");
    gui.activate(assets, "Type"); gui.key(ImGuiKey_DownArrow); gui.key(ImGuiKey_Enter);
    gui.activate(assets, "Sort"); gui.key(ImGuiKey_DownArrow); gui.key(ImGuiKey_Enter);
    gui.capture("filtered");
    gui.text(assets, "Search", "");
    gui.activate(assets, "Type"); gui.key(ImGuiKey_UpArrow); gui.key(ImGuiKey_Enter);
    gui.text(assets, "Thumbnail size", "128", true); gui.key(ImGuiKey_Enter);
    gui.capture("resized");
    gui.activate(assets, "Root");
    gui.item(grid, "many"); gui.key(ImGuiKey_Enter); gui.key(ImGuiKey_End);
    test::expect(ImGui::GetDrawData()->TotalVtxCount < 80000, "Large asset grid is not clipped");
    gui.capture("clipped");
    ImGui::GetIO().AddMousePosEvent(assets->Pos.x + 100, assets->Pos.y + 100); gui.frame(); gui.frame();
    test::expect(gui.ui.frame_state().capture_mouse, "Asset browser did not capture world input");
    // Close with its title-bar X, then reopen the same window through View.
    ImGui::GetIO().AddMousePosEvent(assets->Pos.x + assets->Size.x - 12, assets->Pos.y + 10);
    ImGui::GetIO().AddMouseButtonEvent(0, true); gui.frame();
    ImGui::GetIO().AddMouseButtonEvent(0, false); gui.frame(); gui.frame();
    test::expect(!assets->Active, "Assets window cannot be closed");
    gui.activate(bar, "View");
    ImGuiWindow* menu = nullptr;
    for (auto* candidate : GImGui->Windows)
        if (candidate->Active && (candidate->Flags & ImGuiWindowFlags_Popup)) menu = candidate;
    test::expect(menu != nullptr, "View menu did not open");
    gui.activate(menu, "Assets");
    test::expect(ImGui::FindWindowByName("Assets") == assets && assets->Active, "View did not reopen the same Assets window");
    Uint8 r, g, b, alpha; SDL_ScaleMode mode;
    test::expect(SDL_GetTextureColorMod(tree->texture, &r, &g, &b) && r == 255 && g == 255 && b == 255 &&
                 SDL_GetTextureAlphaMod(tree->texture, &alpha) && alpha == 255 &&
                 SDL_GetTextureScaleMode(tree->texture, &mode) && mode == SDL_SCALEMODE_NEAREST,
                 "Browsing changed shared scene texture state");
    const ImGuiStyle original_style = ImGui::GetStyle();
    ImGui::GetStyle().ScaleAllSizes(1.5f); ImGui::GetStyle().FontScaleDpi = 1.5f;
    gui.frame(); gui.capture("dpi");
    ImGui::GetStyle() = original_style;
    gui.simulation.regenerate(); gui.frame();
    test::expect(gui.textures.load("map/tree1.png") == tree && gui.textures.loaded_count() == 8 &&
                 gui.simulation.clock().ticks() == 0 && gui.simulation.world().actors().size() == before.size(),
                 "Regeneration discarded assets or browsing changed simulation state");
}
}
int main() { return test::run("Asset browser", check_browser); }
