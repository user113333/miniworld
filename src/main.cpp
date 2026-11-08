#include "application_ui.h"
#include "island_demo.h"
#include "map_renderer.h"
#include "tree_renderer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

int main(int, char**)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL initialization failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Miniworld", 1920, 1080,
                                    SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                    &window, &renderer)) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplicationUi ui;
    if (!ui.initialize()) {
        ImGui::DestroyContext();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const bool platform_ready = ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    const bool renderer_ready = platform_ready && ImGui_ImplSDLRenderer3_Init(renderer);
    if (!renderer_ready) {
        SDL_Log("ImGui backend initialization failed");
        if (platform_ready) {
            ImGui_ImplSDL3_Shutdown();
        }
        ImGui::DestroyContext();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    TreeRenderer trees;
    const bool trees_ready = trees.initialize(renderer);
    IslandDemo island;
    bool running = trees_ready;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                 event.window.windowID == SDL_GetWindowID(window))) {
                running = false;
            }
        }
        if (!running) {
            break;
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ui.update_scale(window);
        ImGui::NewFrame();

        // UI actions can replace the map, so fit the view after processing them.
        ui.draw(running, island);

        const ImGuiIO& io = ImGui::GetIO();
        const float margin = 16.0f * ImGui::GetStyle().FontScaleDpi;
        const MapView view = fit_map(island.map(), {margin, margin, io.DisplaySize.x - 2 * margin,
                                                  io.DisplaySize.y - 2 * margin});
        ui.draw_origin_hint(view);

        ImGui::Render();
        // The ImGui renderer backend applies DisplayFramebufferScale itself.
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);
        const SDL_FColor water = terrain_color(Terrain::water);
        SDL_SetRenderDrawColorFloat(renderer, water.r, water.g, water.b, water.a);
        SDL_RenderClear(renderer);
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        draw_map(renderer, island.map(), view);
        trees.draw(renderer, island.map(), view);
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    trees.shutdown();
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return trees_ready ? 0 : 1;
}
