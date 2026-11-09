#include "render/actor_renderer.h"
#include "content_test_support.h"
#include "test_support.h"

namespace {
void check_cache(SDL_Renderer* renderer)
{
    AssetTestDirectory temp;
    temp.copy_resources(std::filesystem::path(SDL_GetBasePath()) / "res");
    temp.write("bad.png", "not a PNG");
    SDL_Surface* source = SDL_CreateSurface(8, 4, SDL_PIXELFORMAT_RGBA32);
    test::expect(source != nullptr, "Cannot create PNG fixture");
    for (int y = 0; y < 4; ++y) for (int x = 0; x < 8; ++x)
        test::expect(SDL_WriteSurfacePixel(source, x, y, 255, 200, 100, x % 2 ? 255 : 0), "Cannot write PNG fixture");
    test::expect(SDL_SavePNG(source, (temp.path / "fixture.png").string().c_str()), "Cannot save PNG fixture");
    SDL_DestroySurface(source);
    content::Catalog content;
    test::expect(content.initialize(temp.path), content.error());
    TextureCache textures; textures.initialize(renderer, content);
    const auto* image = textures.load("fixture.png");
    test::expect(image && image->mask.size == glm::ivec2(8, 4) && content.find("fixture.png")->dimensions == image->mask.size,
                 "Decoded dimensions differ between catalog and mask");
    for (std::size_t i = 0; i < image->mask.alpha.size(); ++i)
        test::expect(image->mask.alpha[i] == (i % 2 ? 255 : 0), "Source alpha mask differs from decoded image");
    SDL_ScaleMode sampling;
    test::expect(SDL_GetTextureScaleMode(image->texture, &sampling) && sampling == SDL_SCALEMODE_NEAREST, "Pixel art is not nearest sampled");
    test::expect(SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) && SDL_RenderClear(renderer), "Cannot clear cache renderer");
    SDL_FRect destination{0, 0, 8, 4};
    test::expect(SDL_RenderTexture(renderer, image->texture, nullptr, &destination) && SDL_RenderPresent(renderer), "Cannot draw cached texture");
    SDL_Surface* rendered = SDL_RenderReadPixels(renderer, nullptr);
    test::expect(rendered != nullptr, "Cannot read cached texture output");
    Uint8 r, g, b, a;
    test::expect(SDL_ReadSurfacePixel(rendered, 1, 0, &r, &g, &b, &a) && r == 255 && g == 200 && b == 100,
                 "Uploaded PNG colors differ from source");
    test::expect(SDL_ReadSurfacePixel(rendered, 0, 0, &r, &g, &b, &a) && r == 0 && g == 0 && b == 0,
                 "Uploaded transparency disagrees with picking mask");
    SDL_DestroySurface(rendered);
    temp.write("fixture.png", "changed after first decode");
    test::expect(textures.load("fixture.png") == image && textures.loaded_count() == 1, "Repeated loading decoded/uploaded a second texture");
    test::expect(!textures.load("bad.png") && content.find("bad.png")->status == content::LoadStatus::failed &&
                 textures.error().find("bad.png") != std::string::npos, "Bad optional preview lacks path diagnostics");
    const auto error = textures.error();
    test::expect(!textures.load("bad.png") && textures.error() == error, "Failed previews are retried every frame");
    test::expect(!textures.load("../fixture.png") && !textures.load("missing.png"), "Invalid/missing keys accepted");
    ActorRenderer actors;
    test::expect(actors.initialize(textures), "Gameplay assets no longer load through cache");
    const auto* tree = textures.load("map/tree1.png");
    test::expect(tree && textures.loaded_count() == 7, "Actor initialization did not share the image cache");
    test::MapState map{12, 12, 0, std::vector<Terrain>(144, Terrain::grass), {{{0, 0}, 0, {}}}};
    map.trees.front().id = 1; map.trees.front().burnout_tick = 640;
    actors.draw(renderer, map, test::actor_views(map), {{80, 100}, 20});
    test::expect(SDL_GetTextureColorMod(tree->texture, &r, &g, &b) && r == 255 && g == 255 && b == 255,
                 "Burning tint leaked into shared preview texture");
    actors.shutdown();
    test::expect(textures.load("map/tree1.png") == tree, "Actor detachment destroyed shared texture");
    textures.shutdown(); textures.shutdown();
    test::expect(textures.loaded_count() == 0 && !textures.load("map/tree1.png"), "Cache shutdown is not safe/idempotent");
    // A missing required sprite leaves consumers detached; the cache owns partial loads.
    std::filesystem::remove(temp.path / "map/tree3.png");
    textures.initialize(renderer, content);
    test::expect(!actors.initialize(textures) && textures.loaded_count() == 2, "Required sprite failure was not explicit");
    actors.shutdown(); textures.shutdown();
}
}
int main()
{
    return test::run("Shared texture cache", [] {
        SDL_Surface* surface = SDL_CreateSurface(240, 140, SDL_PIXELFORMAT_RGBA32);
        test::expect(surface != nullptr, "Cannot create cache test surface");
        SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
        test::expect(renderer != nullptr, "Cannot create software renderer");
        try { check_cache(renderer); }
        catch (...) { SDL_DestroyRenderer(renderer); SDL_DestroySurface(surface); throw; }
        SDL_DestroyRenderer(renderer); SDL_DestroySurface(surface);
    });
}
