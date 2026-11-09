#include "render/actor_renderer.h"
#include "gui/application_ui.h"
#include "gui/thought_bubbles.h"
#include "test_support.h"
#include <set>
#include <cstring>

namespace {
ActorView person(ActorId id, glm::ivec2 hex, SettlerAction action)
{
    ActorView actor{{id, 1}, id, {hex, {}}, {ActorKind::settler, 0}, true, {}, Settler{}};
    actor.settler->action = action;
    if (action == SettlerAction::wander) actor.settler->route = {hex, hex + glm::ivec2{1, 0}};
    return actor;
}
std::vector<unsigned char> read_pixels(SDL_Renderer* renderer)
{
    SDL_Surface* source = SDL_RenderReadPixels(renderer, nullptr);
    test::expect(source != nullptr, "cannot read depth test frame");
    SDL_Surface* rgba = SDL_ConvertSurface(source, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(source);
    test::expect(rgba != nullptr, "cannot convert depth test frame");
    std::vector<unsigned char> pixels(rgba->w * rgba->h * 4);
    for (int y = 0; y < rgba->h; ++y)
        std::memcpy(pixels.data() + y * rgba->w * 4, static_cast<unsigned char*>(rgba->pixels) + y * rgba->pitch, rgba->w * 4);
    SDL_DestroySurface(rgba);
    return pixels;
}
void check_depth(SDL_Renderer* renderer, TextureCache& textures, const ActorSceneView& view)
{
    ActorRenderer scene_renderer;
    SettlerRenderer person_renderer;
    test::expect(scene_renderer.initialize(textures) && person_renderer.initialize(textures), "cannot load depth fixtures");
    auto settler = person(10, {}, SettlerAction::idle);
    ActorView tree{{20, 1}, 20, {{}, {}}, {ActorKind::tree, 0}, true, Tree{}, {}};
    const auto* tree_image = textures.load("map/tree1.png");
    std::array<ActorSpriteMask, 5> masks{};
    masks[0] = tree_image->mask;
    auto clear = [&] { SDL_SetRenderDrawColor(renderer, 88, 116, 72, 255); SDL_RenderClear(renderer); };
    TileMap map{15, 15, 0, std::vector<Terrain>(225, Terrain::grass), {}};
    std::vector<unsigned char> back;
    for (bool foreground_tree : {false, true}) {
        tree.position.offset.y = foreground_tree ? .1f : -.1f;
        clear();
        scene_renderer.draw(renderer, map, {tree, settler}, {view.origin, view.radius}, nullptr, 0, 1);
        const auto actual = read_pixels(renderer);
        std::vector<ActorSceneEntry> trees;
        build_actor_scene(map, {tree}, view, masks, trees);
        person_renderer.prepare({settler}, view, 0, 1);
        const auto& t = trees.front();
        const SDL_FRect destination{t.minimum.x, t.minimum.y, t.size.x, t.size.y};
        auto draw_tree = [&] { SDL_RenderTexture(renderer, tree_image->texture, nullptr, &destination); };
        auto draw_person = [&] { person_renderer.draw(renderer, person_renderer.entries().front()); };
        clear();
        if (foreground_tree) { draw_person(); draw_tree(); } else { draw_tree(); draw_person(); }
        test::expect(read_pixels(renderer) == actual, "settlers and trees do not share scene depth order");
        if (!foreground_tree) back = actual;
        else test::expect(back != actual, "depth fixture has no overlap");
    }
}
void check(SDL_Window* window, SDL_Renderer* renderer)
{
    content::Catalog catalog;
    test::expect(catalog.initialize(std::filesystem::path(SDL_GetBasePath()) / "res"), catalog.error());
    TextureCache textures;
    textures.initialize(renderer, catalog);
    const std::string key = "minifolks/villagers2/MiniLumberjack.aseprite";
    const auto* animation = textures.load_animation(key);
    test::expect(animation && animation->frames.size() == 45 && animation->frames[0]->mask.size == glm::ivec2(32), "sprite decode failed");
    test::expect(textures.load_animation(key) == animation && textures.loaded_count() == 1, "animation cache did not reuse frames");
    test::expect(catalog.find(key)->status == content::LoadStatus::loaded, "catalog misses loaded animation");
    test::expect(animation->clips.at("idle").duration_ms == 400 && animation->clips.at("idle2").duration_ms == 900 &&
                 animation->clips.at("work").duration_ms == 1000 && animation->clips.at("death").duration_ms == 900, "frame durations lost");
    const auto& work = animation->clips.at("work");
    test::expect(animation->frame_at(work, 199) == 12 && animation->frame_at(work, 200) == 13 &&
                 animation->frame_at(work, 1000) == 12, "frame boundaries or looping wrong");
    for (const auto& frame : animation->frames) {
        SDL_ScaleMode mode;
        test::expect(SDL_GetTextureScaleMode(frame->texture, &mode) && mode == SDL_SCALEMODE_NEAREST, "animation must use nearest sampling");
        test::expect(frame->mask.alpha[0] == 0, "transparent padding lost");
    }
    SettlerRenderer settlers;
    test::expect(settlers.initialize(textures), "settler renderer failed");
    const ActorSceneView view{{320, 220}, 40, {1, 1}};
    auto idle = person(1, {-2, 0}, SettlerAction::idle);
    auto walking = person(2, {}, SettlerAction::wander);
    auto cutting = person(3, {2, 0}, SettlerAction::cut);
    std::vector<ActorView> actors{idle, walking, cutting};
    settlers.prepare(actors, view, 0, 1);
    for (const auto& entry : settlers.entries()) {
        if (entry.id == 2) test::expect(entry.frame == 19, "walking uses wrong clip");
        if (entry.id == 3) test::expect(entry.frame == 12, "cutting uses wrong clip");
    }
    settlers.prepare(actors, view, 8, 1);
    for (const auto& entry : settlers.entries()) if (entry.id == 2) test::expect(entry.frame == 21, "walk does not advance on simulation clock");
    const auto paused = settlers.entries();
    settlers.prepare(actors, view, 8, 1);
    for (std::size_t i = 0; i < paused.size(); ++i) test::expect(paused[i].frame == settlers.entries()[i].frame, "paused animation changes");
    actors[1].position.offset.x -= .25f;
    settlers.prepare(actors, view, 9, 1);
    for (const auto& entry : settlers.entries()) if (entry.id == 2) test::expect(entry.flipped, "walker does not face movement");
    std::set<std::size_t> idle_frames;
    for (int tick = 10; tick < 400; ++tick) {
        settlers.prepare(actors, view, tick, 1);
        for (const auto& entry : settlers.entries()) if (entry.id == 1) idle_frames.insert(entry.frame);
    }
    test::expect(*idle_frames.begin() < 4 && *idle_frames.rbegin() >= 4, "resting never chooses both idle clips");
    auto zoomed = settler_layout(idle.position, {{100, 100}, 80, {1.5f, 1.5f}});
    test::expect(test::near(zoomed.size.x, settlers.entries()[0].layout.size.x * 2), "sprite does not scale with camera");
    settlers.prepare({}, view, 400, 1);
    test::expect(settlers.entries().size() == 3, "removed settlers lack death animation");
    for (const auto& entry : settlers.entries()) test::expect(entry.frame == 40, "death starts on wrong frame");
    settlers.prepare({}, view, 428, 1);
    test::expect(settlers.entries().size() == 3, "death ends early");
    settlers.prepare({}, view, 429, 1);
    test::expect(settlers.entries().empty(), "death leaves persistent remains");
    settlers.prepare(actors, view, 430, 1);
    settlers.prepare({}, view, 0, 2);
    test::expect(settlers.entries().empty(), "regeneration plays old-world deaths");

    check_depth(renderer, textures, view);

    ApplicationUi ui;
    test::expect(ui.initialize(window, renderer, catalog, textures), "cannot create bubble UI");
    ImGui::GetIO().IniFilename = nullptr;
    for (auto& actor : actors) { actor.settler->thought = "I like it here"; actor.settler->thought_ticks = 320; }
    auto bubbles = [&](bool visible) {
        ui.begin_frame(window);
        for (auto& actor : actors) actor.settler->thought_ticks = visible ? 1 : 0;
        draw_thought_bubbles(actors, view);
        ui.finish_frame();
        return ImGui::GetDrawData()->TotalVtxCount;
    };
    test::expect(bubbles(true) > 0, "active thoughts do not draw bubbles");
    test::expect(bubbles(false) == 0, "expired thought remains visible");
    bubbles(true);
    SDL_SetRenderDrawColor(renderer, 88, 116, 72, 255); SDL_RenderClear(renderer);
    ActorRenderer renderer_actors;
    test::expect(renderer_actors.initialize(textures), "cannot load scene assets");
    TileMap map{15, 15, 0, std::vector<Terrain>(225, Terrain::grass), {}};
    renderer_actors.draw(renderer, map, actors, {view.origin, view.radius}, nullptr, 8, 1);
    ui.render(renderer);
    SDL_RenderPresent(renderer);
    SDL_Surface* pixels = SDL_RenderReadPixels(renderer, nullptr);
    test::expect(pixels != nullptr, "cannot capture settler frame");
    test::expect(SDL_SavePNG(pixels, "/tmp/miniworld-settler-render.png"), "cannot save review frame");
    SDL_DestroySurface(pixels);
    renderer_actors.shutdown(); ui.shutdown(); settlers.shutdown(); textures.shutdown();
    test::expect(!textures.load_animation(key), "detached animation cache accepted load");
}
}
int main() { return test::run("Settler rendering", [] {
    test::expect(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
    SDL_Window* window = SDL_CreateWindow("Settler review", 640, 320, SDL_WINDOW_HIDDEN);
    test::expect(window != nullptr, SDL_GetError());
    SDL_Renderer* renderer = SDL_CreateRenderer(window, "software");
    test::expect(renderer != nullptr, SDL_GetError());
    try { check(window, renderer); }
    catch (...) { SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); throw; }
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
}); }
