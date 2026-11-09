#pragma once

#include "entity_explorer.h"
#include "entity_tool.h"
#include "asset_browser.h"
#include "settlers_window.h"

#include <glm/glm.hpp>
#include <imgui.h>

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;
class Simulation;
struct MapView;
class Camera;

struct GuiFrameState {
    glm::vec2 display_size;
    glm::vec2 framebuffer_scale;
    float content_scale;
    bool capture_mouse;
};

class ApplicationUi {
public:
    ApplicationUi() = default;
    ~ApplicationUi();
    ApplicationUi(const ApplicationUi&) = delete;
    ApplicationUi& operator=(const ApplicationUi&) = delete;

    bool initialize(SDL_Window* window, SDL_Renderer* renderer,
                    content::Catalog& content, TextureCache& textures);
    void shutdown();
    void process_event(const SDL_Event& event);
    void begin_frame(SDL_Window* window);
    GuiFrameState frame_state() const;
    void draw(bool& running, Simulation& simulation);
    void select_actor(ActorHandle actor);
    void spawn_settler_at(Simulation& simulation, const MapView& view, glm::vec2 mouse, std::uint64_t generation);
    ActorHandle selected_actor() const { return entity_explorer_.selected(); }
    EntityTool tool() const { return tool_; }
    void cancel_tool() { tool_ = EntityTool::none; }
    void draw_overlays(const Simulation& simulation, const MapView& view);
    void update_camera(Camera& camera, const MapView& centered);
    void finish_frame();
    void render(SDL_Renderer* renderer) const;

private:
    bool load_font();
    void update_scale(SDL_Window* window);
    void draw_origin_hint(const MapView& view) const;

    ImGuiContext* context_ = nullptr;
    bool platform_ready_ = false;
    bool renderer_ready_ = false;
    ImGuiStyle base_style_;
    EntityExplorer entity_explorer_;
    AssetBrowser asset_browser_;
    SettlersWindow settlers_window_;
    content::Catalog* content_ = nullptr;
    TextureCache* textures_ = nullptr;
    std::vector<unsigned char> font_data_;
    EntityTool tool_ = EntityTool::none;
    std::uint64_t world_generation_ = 0;
    float content_scale_ = 0.0f;
    bool camera_dragging_ = false;
    bool show_demo_window_ = false;
    bool show_island_window_ = true;
    bool show_simulation_window_ = true;
};
