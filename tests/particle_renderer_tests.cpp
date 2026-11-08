#include "render/particle_renderer.h"
#include "test_support.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace {

class SoftwareCanvas {
public:
    SoftwareCanvas()
    {
        surface = SDL_CreateSurface(320, 240, SDL_PIXELFORMAT_RGBA32);
        test::expect(surface != nullptr, "Cannot create particle test surface");
        renderer = SDL_CreateSoftwareRenderer(surface);
        test::expect(renderer != nullptr, "Cannot create software particle renderer");
    }

    ~SoftwareCanvas()
    {
        SDL_DestroyRenderer(renderer);
        SDL_DestroySurface(surface);
    }

    SDL_Surface* surface = nullptr;
    SDL_Renderer* renderer = nullptr;
};

void expect_unchanged(const ParticleSystem& system, const Particle& before,
                       std::uint64_t spawned, std::uint64_t dropped)
{
    test::expect(system.size() == 1 && system.spawned_count() == spawned &&
                 system.dropped_count() == dropped, "Drawing changed particle counters");
    const Particle& after = system.data()[0];
    test::expect(after.position == before.position && after.velocity == before.velocity &&
                 after.ground == before.ground && after.color == before.color &&
                 after.size == before.size && after.age == before.age &&
                 after.lifetime == before.lifetime && after.kind == before.kind &&
                 after.emitter_id == before.emitter_id && after.birth_ordinal == before.birth_ordinal,
                 "Drawing changed particle state");
}

void check_footprint(SoftwareCanvas& canvas, const ParticleSystem& system,
                      ActorSceneView view)
{
    test::expect(SDL_SetRenderScale(canvas.renderer, view.framebuffer_scale.x, view.framebuffer_scale.y),
                 "Cannot set particle test DPI");
    test::expect(SDL_SetRenderDrawColor(canvas.renderer, 40, 70, 100, 255) &&
                 SDL_RenderClear(canvas.renderer), "Cannot clear particle test output");
    test::expect(SDL_SetRenderDrawBlendMode(canvas.renderer, SDL_BLENDMODE_ADD),
                 "Cannot set initial blend state");

    const Particle before = system.data()[0];
    const std::uint64_t spawned = system.spawned_count();
    const std::uint64_t dropped = system.dropped_count();
    ParticleRenderer renderer;
    renderer.prepare(system, view);
    test::expect(renderer.groups().size() == 1, "One emitter did not produce one draw group");
    renderer.draw(canvas.renderer, renderer.groups()[0], ParticleKind::flame, view);
    SDL_BlendMode blend;
    test::expect(SDL_GetRenderDrawBlendMode(canvas.renderer, &blend) && blend == SDL_BLENDMODE_ADD,
                 "Particle rendering did not restore blend state");
    test::expect(SDL_RenderPresent(canvas.renderer), "Cannot flush particle test output");
    expect_unchanged(system, before, spawned, dropped);

    // Expected half-open square in physical framebuffer pixels. SDL's software
    // quad path used to lose an edge when logical width*DPI rounded below an integer.
    const float art_scale = glm::sqrt(3.0f) * view.radius *
                            view.framebuffer_scale.x * 1.05f / 8.0f;
    const int side = static_cast<int>(glm::max(1.0f, glm::round(before.size * art_scale)));
    const glm::ivec2 minimum(glm::round((view.origin + before.position * view.radius)
                                       * view.framebuffer_scale - side * 0.5f));
    int changed = 0;
    for (int y = 0; y < canvas.surface->h; ++y) {
        for (int x = 0; x < canvas.surface->w; ++x) {
            Uint8 red, green, blue, alpha;
            test::expect(SDL_ReadSurfacePixel(canvas.surface, x, y, &red, &green, &blue, &alpha),
                         "Cannot read particle test output");
            const bool painted = red != 40 || green != 70 || blue != 100;
            const bool expected = x >= minimum.x && x < minimum.x + side &&
                                  y >= minimum.y && y < minimum.y + side;
            test::expect(painted == expected, "Particle square lost an edge or painted outside its footprint");
            changed += painted;
        }
    }
    const int width = std::max(0, std::min(canvas.surface->w, minimum.x + side) - std::max(0, minimum.x));
    const int height = std::max(0, std::min(canvas.surface->h, minimum.y + side) - std::max(0, minimum.y));
    test::expect(changed == width * height && changed > 0, "Particle square was not physically square");
}

} // namespace

int main()
{
    return test::run("Particle square rendering", [] {
        SoftwareCanvas canvas;
        ParticleSystem system(1);
        system.reset(42);
        system.set_emitters({{1, {0, 0}, 1.5f, 0, 640}});
        for (std::uint64_t tick = 1; tick < 128 && system.size() == 0; ++tick) {
            system.step(tick);
        }
        test::expect(system.size() == 1 && system.data()[0].kind == ParticleKind::flame,
                     "Fixture did not produce a real flame particle");
        for (glm::vec2 scale : {glm::vec2{1}, {1.25f, 1.25f}, {1.5f, 1.5f},
                                {2, 2}, {1.25f, 2}}) {
            check_footprint(canvas, system, {{80.3f, 70.7f}, 12.5f, scale});
            const glm::vec2 clipped_origin = -system.data()[0].position * 12.5f
                                             + glm::vec2{0.25f} / scale;
            check_footprint(canvas, system, {clipped_origin, 12.5f, scale});
        }
    });
}
