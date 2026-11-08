#include "application.h"
#include "map_renderer.h"

#include <SDL3/SDL.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

Application::~Application()
{
    // Textures and ImGui backends must be released before their SDL renderer.
    trees_.shutdown();
    if (imgui_renderer_ready_) {
        ImGui_ImplSDLRenderer3_Shutdown();
    }
    if (imgui_platform_ready_) {
        ImGui_ImplSDL3_Shutdown();
    }
    if (imgui_context_) {
        ImGui::DestroyContext(imgui_context_);
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_) {
        SDL_DestroyWindow(window_);
    }
    if (sdl_ready_) {
        SDL_Quit();
    }
}

bool Application::initialize()
{
    sdl_ready_ = SDL_Init(SDL_INIT_VIDEO);
    if (!sdl_ready_) {
        SDL_Log("SDL initialization failed: %s", SDL_GetError());
        return false;
    }
    if (!SDL_CreateWindowAndRenderer("Miniworld", 1920, 1080,
                                    SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                    &window_, &renderer_)) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(renderer_, 1);

    IMGUI_CHECKVERSION();
    imgui_context_ = ImGui::CreateContext();
    if (!ui_.initialize()) {
        return false;
    }
    imgui_platform_ready_ = ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
    imgui_renderer_ready_ = imgui_platform_ready_ && ImGui_ImplSDLRenderer3_Init(renderer_);
    if (!imgui_renderer_ready_) {
        SDL_Log("ImGui backend initialization failed");
        return false;
    }
    if (!trees_.initialize(renderer_)) {
        return false;
    }
    island_.emplace();
    return true;
}

int Application::run()
{
    if (!initialize()) {
        return 1;
    }
    running_ = true;
    while (running_) {
        running_ = process_events();
        if (!running_) {
            break;
        }
        if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }
        draw_frame();
    }
    return 0;
}

bool Application::process_events()
{
    bool running = true;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT ||
            (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
             event.window.windowID == SDL_GetWindowID(window_))) {
            running = false;
        }
    }
    return running;
}

void Application::draw_frame()
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ui_.update_scale(window_);
    ImGui::NewFrame();

    // UI actions can replace the map, so fit the view after processing them.
    ui_.draw(running_, *island_);
    const ImGuiIO& io = ImGui::GetIO();
    const float margin = 16.0f * ImGui::GetStyle().FontScaleDpi;
    const MapView view = fit_map(island_->map(), {margin, margin, io.DisplaySize.x - 2 * margin,
                                               io.DisplaySize.y - 2 * margin});
    ui_.draw_origin_hint(view);

    ImGui::Render();
    // The ImGui renderer backend applies DisplayFramebufferScale itself.
    SDL_SetRenderScale(renderer_, 1.0f, 1.0f);
    const SDL_FColor water = terrain_color(Terrain::water);
    SDL_SetRenderDrawColorFloat(renderer_, water.r, water.g, water.b, water.a);
    SDL_RenderClear(renderer_);
    SDL_SetRenderScale(renderer_, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    draw_map(renderer_, island_->map(), view);
    trees_.draw(renderer_, island_->map(), view);
    SDL_SetRenderScale(renderer_, 1.0f, 1.0f);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
    SDL_RenderPresent(renderer_);
}
