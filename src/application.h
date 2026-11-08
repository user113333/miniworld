#pragma once

#include "application_ui.h"
#include "island_demo.h"
#include "tree_renderer.h"

#include <optional>

class Application {
public:
    Application() = default;
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    bool initialize();
    bool process_events();
    void draw_frame();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    ImGuiContext* imgui_context_ = nullptr;
    bool sdl_ready_ = false;
    bool imgui_platform_ready_ = false;
    bool imgui_renderer_ready_ = false;
    bool running_ = false;

    ApplicationUi ui_;
    TreeRenderer trees_;
    std::optional<IslandDemo> island_;
};
