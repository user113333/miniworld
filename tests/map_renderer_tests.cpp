#include "map_renderer.h"
#include "test_support.h"

#include <array>

namespace {

using Pixel = std::array<Uint8, 4>;

void check_solid_tile(SDL_Renderer* renderer, SDL_Surface* surface,
                       const TileMap& map, Pixel expected)
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
    TileMap map{12, 12, 42, std::vector<Terrain>(144, Terrain::water), {}};
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
