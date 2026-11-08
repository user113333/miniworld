#pragma once

#include <imgui.h>

struct SDL_Window;
class IslandDemo;
struct MapView;

class ApplicationUi {
public:
    bool initialize();
    void update_scale(SDL_Window* window);
    void draw(bool& running, IslandDemo& island);
    void draw_origin_hint(const MapView& view) const;

private:
    ImGuiStyle base_style_;
    float content_scale_ = 0.0f;
    bool show_demo_window_ = false;
    bool show_island_window_ = true;
};
