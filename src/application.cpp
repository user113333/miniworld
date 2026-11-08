#include "application.h"
#include "map_renderer.h"

#include <SDL3/SDL.h>

#include <chrono>

Application::~Application()
{
    // Textures and ImGui backends must be released before their SDL renderer.
    trees_.shutdown();
    ui_.shutdown();
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

    if (!ui_.initialize(window_, renderer_)) {
        return false;
    }
    if (!trees_.initialize(renderer_)) {
        return false;
    }
    island_.emplace();
    particles_.reset(island_->map().seed);
    effects_generation_ = island_->world_generation();
    return true;
}

int Application::run()
{
    if (!initialize()) {
        return 1;
    }
    running_ = true;
    auto previous_frame = std::chrono::steady_clock::now();
    bool was_minimized = false;
    while (running_) {
        const auto now = std::chrono::steady_clock::now();
        const double real_seconds = std::chrono::duration<double>(now - previous_frame).count();
        previous_frame = now;
        running_ = process_events();
        if (!running_) {
            break;
        }
        if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) {
            was_minimized = true;
            SDL_Delay(10);
            continue;
        }
        // Discard the final inactive interval as well as all minimized frames.
        draw_frame(was_minimized ? 0.0 : real_seconds);
        was_minimized = false;
    }
    return 0;
}

bool Application::process_events()
{
    world_clicks_.clear();
    bool running = true;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ui_.process_event(event);
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
            island_->settings.select_entities = false;
            island_->settings.ignite_trees = false;
            world_clicks_.clear();
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
            world_clicks_.push_back({{event.button.x, event.button.y}, island_->world_generation()});
        }
        if (event.type == SDL_EVENT_QUIT ||
            (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
             event.window.windowID == SDL_GetWindowID(window_))) {
            running = false;
        }
    }
    return running;
}

void Application::draw_frame(double real_seconds)
{
    ui_.begin_frame(window_);

    // UI actions can replace the map, so fit the view after processing them.
    const auto generation_before_ui = island_->world_generation();
    ui_.draw(running_, *island_);
    const GuiFrameState frame = ui_.frame_state();
    const float margin = 16.0f * frame.content_scale;
    const MapView view = fit_map(island_->map(), {margin, margin, frame.display_size.x - 2 * margin,
                                               frame.display_size.y - 2 * margin});
    resolve_world_clicks(view, frame.capture_mouse, frame.framebuffer_scale);
    // Time measured before regeneration belongs to the old world.
    advance_simulation(island_->world_generation() == generation_before_ui ? real_seconds : 0.0);
    ui_.draw_overlays(*island_, view);

    ui_.finish_frame();
    SDL_SetRenderScale(renderer_, 1.0f, 1.0f);
    const SDL_FColor water = terrain_color(Terrain::water);
    SDL_SetRenderDrawColorFloat(renderer_, water.r, water.g, water.b, water.a);
    SDL_RenderClear(renderer_);
    SDL_SetRenderScale(renderer_, frame.framebuffer_scale.x, frame.framebuffer_scale.y);
    draw_map(renderer_, island_->map(), view);
    trees_.draw(renderer_, island_->map(), view, &particles_);
    SDL_SetRenderScale(renderer_, 1.0f, 1.0f);
    ui_.render(renderer_);
    SDL_RenderPresent(renderer_);
}

void Application::resolve_world_clicks(const MapView& view, bool capture_mouse,
                                      glm::vec2 framebuffer_scale)
{
    if (!capture_mouse && (island_->settings.select_entities || island_->settings.ignite_trees)) {
        for (const WorldClick& click : world_clicks_) {
            if (click.generation != island_->world_generation()) {
                continue;
            }
            const auto id = trees_.pick(island_->map(), view, framebuffer_scale, click.position);
            if (island_->settings.select_entities) {
                ui_.select_tree(id, click.generation);
            } else {
                island_->queue_ignition(id, click.generation);
            }
        }
    }
    world_clicks_.clear();
}

void Application::advance_simulation(double real_seconds)
{
    if (effects_generation_ != island_->world_generation()) {
        particles_.reset(island_->map().seed);
        fire_emitters_.clear();
        effects_generation_ = island_->world_generation();
    }
    island_->accumulate_simulation_time(real_seconds);
    while (island_->step_simulation()) {
        if (island_->clock().world_update_due()) {
            collect_fire_emitters(island_->map(), fire_emitters_);
            particles_.set_emitters(fire_emitters_);
        }
        particles_.step(island_->clock().ticks());
    }
}
