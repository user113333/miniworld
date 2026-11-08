#pragma once

#include "tree_inspector.h"

#include <glm/glm.hpp>
#include <imgui.h>

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;
class IslandDemo;
struct MapView;

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

    bool initialize(SDL_Window* window, SDL_Renderer* renderer);
    void shutdown();
    void process_event(const SDL_Event& event);
    void begin_frame(SDL_Window* window);
    GuiFrameState frame_state() const;
    void draw(bool& running, IslandDemo& island);
    void select_tree(std::uint64_t tree_id, std::uint64_t generation);
    void draw_overlays(const IslandDemo& island, const MapView& view);
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
    TreeInspector tree_inspector_;
    float content_scale_ = 0.0f;
    bool show_demo_window_ = false;
    bool show_island_window_ = true;
    bool show_simulation_window_ = true;
};
