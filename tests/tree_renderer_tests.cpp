#include "render/actor_renderer.h"
#include "test_support.h"

#include <array>
#include <vector>

namespace {

using Pixel = std::array<Uint8, 4>;

std::vector<Pixel> render(SDL_Renderer* renderer, SDL_Surface* surface,
                           const ActorRenderer& trees, const test::MapState& map)
{
    test::expect(SDL_SetRenderDrawColor(renderer, 40, 70, 100, 255) && SDL_RenderClear(renderer),
                 "Cannot clear tree test surface");
    trees.draw(renderer, map, test::actor_views(map), {{80, 100}, 20});
    test::expect(SDL_RenderPresent(renderer), "Cannot flush tree drawing");
    std::vector<Pixel> pixels(surface->w * surface->h);
    for (int y = 0; y < surface->h; ++y) {
        for (int x = 0; x < surface->w; ++x) {
            auto& pixel = pixels[y * surface->w + x];
            test::expect(SDL_ReadSurfacePixel(surface, x, y, &pixel[0], &pixel[1], &pixel[2], &pixel[3]),
                         "Cannot read tree test pixel");
        }
    }
    return pixels;
}

void check_tint(SDL_Renderer* renderer, SDL_Surface* surface)
{
    content::Catalog content;
    test::expect(content.initialize(std::filesystem::path(SDL_GetBasePath()) / "res"), content.error());
    TextureCache textures;
    textures.initialize(renderer, content);
    ActorRenderer trees;
    test::expect(trees.initialize(textures), "Cannot load tree sprites for tint test");
    test::MapState map{12, 12, 42, std::vector<Terrain>(144, Terrain::grass),
                 {{{0, 0}, 0, {}}, {{3, 0}, 0, {}}}};
    map.trees[0].id = 1;
    map.trees[1].id = 2;
    for (unsigned variant : {0u, 1u, 2u}) {
        for (test::TreeRecord& tree : map.trees) {
            tree.variant = variant;
        }
        const auto normal = render(renderer, surface, trees, map);
        map.trees[0].ignition_tick = 8;
        map.trees[0].burnout_tick = 640;
        const auto burning = render(renderer, surface, trees, map);
        test::expect(render(renderer, surface, trees, map) == burning,
                     "Drawing without simulation changed the burning cue");
        int changed = 0;
        for (int y = 0; y < surface->h; ++y) {
            for (int x = 0; x < surface->w; ++x) {
                const auto index = y * surface->w + x;
                if (normal[index] == burning[index]) {
                    continue;
                }
                ++changed;
                test::expect(x < 120, "Burning tint leaked to a neighboring tree sharing its sprite");
                test::expect(burning[index][0] == normal[index][0] &&
                             burning[index][1] <= normal[index][1] &&
                             burning[index][2] <= normal[index][2],
                             "Burning sprite did not receive a warm red cast");
            }
        }
        test::expect(changed > 0, "Burning tree has no visible color cue without particles");
        test::expect(map.trees[0].ignition_tick == 8 && map.trees[0].burnout_tick == 640,
                     "Rendering changed authoritative fire state");
        map.trees[0].burnout_tick = 0;
        test::expect(render(renderer, surface, trees, map) == normal,
                     "Extinguished tree retained the burning tint");
    }
}

} // namespace

int main()
{
    return test::run("Burning tree appearance", [] {
        SDL_Surface* surface = SDL_CreateSurface(240, 140, SDL_PIXELFORMAT_RGBA32);
        test::expect(surface != nullptr, "Cannot create tree test surface");
        SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
        if (!renderer) {
            SDL_DestroySurface(surface);
            test::expect(false, "Cannot create tree software renderer");
        }
        try {
            check_tint(renderer, surface);
        } catch (...) {
            SDL_DestroyRenderer(renderer);
            SDL_DestroySurface(surface);
            throw;
        }
        SDL_DestroyRenderer(renderer);
        SDL_DestroySurface(surface);
    });
}
