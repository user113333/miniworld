#include "render/map_renderer.h"
#include "render/camera.h"
#include "test_support.h"

#include <array>

namespace {

using Pixel = std::array<Uint8, 4>;

void check_solid_tile(SDL_Renderer* renderer, SDL_Surface* surface,
                       const test::MapState& map, Pixel expected)
{
    const auto terrain = map.tiles;
    const auto recovery = map.recovery;
    test::expect(SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255) &&
                 SDL_RenderClear(renderer), "Cannot clear terrain test surface");
    draw_map(renderer, map, {{120, 100}, 24});
    test::expect(SDL_RenderPresent(renderer), "Cannot flush terrain drawing");
    // A broad interior patch of the origin hex must have one exact flat color.
    for (int y = 92; y <= 108; ++y) {
        for (int x = 112; x <= 128; ++x) {
            Pixel actual;
            test::expect(SDL_ReadSurfacePixel(surface, x, y, &actual[0], &actual[1],
                                              &actual[2], &actual[3]),
                         "Cannot read terrain pixel");
            test::expect(actual == expected, "Origin hex has an incorrect or non-solid terrain color");
        }
    }
    test::expect(map.tiles == terrain, "Drawing changed canonical terrain");
    test::expect(map.recovery.size() == recovery.size(), "Drawing resized recovery state");
    for (std::size_t i = 0; i < recovery.size(); ++i) {
        const auto& before = recovery[i];
        const auto& after = map.recovery[i];
        test::expect(before.original_terrain == after.original_terrain &&
                     before.started_tick == after.started_tick &&
                     before.duration_ticks == after.duration_ticks &&
                     before.elapsed_ticks == after.elapsed_ticks,
                     "Drawing advanced recovery state");
    }
}

void check_recovery_colors(SDL_Renderer* renderer, SDL_Surface* surface)
{
    test::MapState map{12, 12, 42, std::vector<Terrain>(144, Terrain::water), {}};
    const int origin = map.index({0, 0});
    const std::array<Terrain, 4> original_terrain{
        Terrain::water, Terrain::sand, Terrain::land, Terrain::grass};
    const std::array<Pixel, 4> original_colors{{
        {89, 113, 135, 255}, {197, 173, 103, 255},
        {168, 144, 91, 255}, {147, 159, 75, 255}
    }};
    // Ordinary terrain and worlds without runtime recovery metadata retain
    // the approved palette and origin geometry.
    for (std::size_t i = 0; i < original_terrain.size(); ++i) {
        map.tiles[origin] = original_terrain[i];
        check_solid_tile(renderer, surface, map, original_colors[i]);
    }
    map.recovery.resize(map.tiles.size());
    for (Terrain original : {Terrain::land, Terrain::grass}) {
        map.tiles[origin] = Terrain::burned;
        map.recovery[origin] = {original, 8, 128, 0};
        check_solid_tile(renderer, surface, map, {91, 78, 69, 255});
        map.recovery[origin].elapsed_ticks = 63;
        check_solid_tile(renderer, surface, map, {91, 78, 69, 255});
        map.recovery[origin].elapsed_ticks = 64;
        check_solid_tile(renderer, surface, map, {138, 121, 102, 255});
        map.recovery[origin].elapsed_ticks = 127;
        check_solid_tile(renderer, surface, map, {138, 121, 102, 255});
        test::expect(map.tiles[origin] == Terrain::burned,
                     "Weathering changed gameplay terrain before completion");
        map.tiles[origin] = original;
        map.recovery[origin] = {};
        check_solid_tile(renderer, surface, map, original == Terrain::land
            ? Pixel{168, 144, 91, 255} : Pixel{147, 159, 75, 255});
    }
}

} // namespace

int main()
{
    return test::run("Burned terrain rendering", [] {
        const MapView small = centered_map_view({16, 16, 608, 448});
        const MapView large = centered_map_view({16, 16, 1888, 1048});
        test::expect(small.origin == glm::vec2(320, 240) &&
                     large.origin == glm::vec2(960, 540), "Resizing must center the world");
        test::expect(small.radius == large.radius, "Resizing must preserve world scale");
        Camera camera;
        camera.pan({40, -30});
        test::expect(camera.view(small).origin == small.origin + glm::vec2(40, -30),
                     "Camera drag must translate the view");
        const glm::vec2 cursor{170, 180};
        const auto before = camera.view(small);
        const glm::vec2 world = (cursor - before.origin) / before.radius;
        camera.zoom(2, cursor, small);
        const auto zoomed = camera.view(small);
        test::expect(zoomed.radius > before.radius &&
                     glm::length(zoomed.origin + world * zoomed.radius - cursor) < 0.001f,
                     "Zoom must preserve the world point under the cursor");
        test::expect(camera.view(large).radius == zoomed.radius,
                     "Resizing must preserve camera zoom");
        camera.zoom(10000, cursor, small);
        test::expect(camera.view(small).radius == small.radius * 8,
                     "Camera zoom must have an upper bound");
        camera.zoom(-10000, cursor, small);
        test::expect(camera.view(small).radius == small.radius * 0.25f,
                     "Camera zoom must have a lower bound");
        SDL_Surface* surface = SDL_CreateSurface(240, 200, SDL_PIXELFORMAT_RGBA32);
        test::expect(surface != nullptr, "Cannot create terrain test surface");
        SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
        if (!renderer) {
            SDL_DestroySurface(surface);
            test::expect(false, "Cannot create terrain software renderer");
        }
        try {
            check_recovery_colors(renderer, surface);
        } catch (...) {
            SDL_DestroyRenderer(renderer);
            SDL_DestroySurface(surface);
            throw;
        }
        SDL_DestroyRenderer(renderer);
        SDL_DestroySurface(surface);
    });
}
