#pragma once

#include "gui/application_ui.h"
#include "simulation/simulation.h"
#include "effects/fire_effects.h"
#include "render/actor_renderer.h"

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
    void draw_frame(double real_seconds);
    void advance_simulation(double real_seconds);
    void resolve_world_clicks(const MapView& view, bool capture_mouse, glm::vec2 framebuffer_scale);

    struct WorldClick {
        glm::vec2 position;
        std::uint64_t generation;
    };

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool sdl_ready_ = false;
    bool running_ = false;

    ApplicationUi ui_;
    ActorRenderer actors_;
    std::optional<Simulation> simulation_;
    ParticleSystem particles_;
    std::vector<FireEmitter> fire_emitters_;
    std::vector<WorldClick> world_clicks_;
    std::uint64_t effects_generation_ = 0;
};
