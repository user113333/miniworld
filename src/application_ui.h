#pragma once

#include <imgui.h>

struct SDL_Window;

class ApplicationUi {
public:
    bool initialize();
    void update_scale(SDL_Window* window);
    void draw(bool& running);

private:
    ImGuiStyle base_style_;
    float content_scale_ = 0.0f;
    bool show_demo_window_ = false;
};
