#include "simulation/simulation.h"
#include "world/island_generator.h"
#include "world/tree_placement.h"
#include "test_support.h"

#include <cstring>
#include <fstream>

namespace {

struct Digest {
    std::uint64_t value = 14695981039346656037ULL;
    void add(std::uint64_t x)
    {
        for (int i = 0; i < 8; ++i) { value = (value ^ (x & 255)) * 1099511628211ULL; x >>= 8; }
    }
    void real(float x) { std::uint32_t bits; std::memcpy(&bits, &x, sizeof(bits)); add(bits); }
};

std::uint64_t digest(const World& world)
{
    Digest d;
    const auto& map = world.map();
    d.add(map.width); d.add(map.height); d.add(map.seed);
    for (auto terrain : map.tiles) d.add(static_cast<unsigned>(terrain));
    for (const auto& r : map.recovery) {
        d.add(static_cast<unsigned>(r.original_terrain)); d.add(r.started_tick);
        d.add(r.duration_ticks); d.add(r.elapsed_ticks);
    }
    const auto actors = world.actors(); // Stable gameplay-ID order, independent of ECS tables.
    d.add(actors.size());
    for (const auto& actor : actors) {
        const auto& t = *actor.tree;
        d.add(actor.id); d.add(actor.position.hex.x); d.add(actor.position.hex.y);
        d.add(actor.appearance.variant); d.real(actor.position.offset.x); d.real(actor.position.offset.y);
        d.add(t.decay_ticks); d.add(t.dead_ticks);
        d.add(t.ignition_tick); d.add(t.burnout_tick); d.add(t.charred);
    }
    return d.value;
}

void check_runtime_replay()
{
    std::ifstream fixture(std::string(TEST_FIXTURE_DIR) + "/flecs_runtime.txt");
    test::expect(fixture.is_open(), "Cannot open runtime replay fixture");
    for (std::uint32_t seed : {0u, 74129u, 4294967295u}) {
        auto map = generate_island(24, 18, seed);
        const auto spawns = place_trees(map, 1.0f);
        World world;
        world.reset(map, spawns);
        const auto target = [&](std::size_t i) { return world.actor_at(map.index(spawns[i].hex)); };
        const auto middle = target(spawns.size() / 2);
        test::expect(world.enqueue({target(0)}) && world.enqueue({target(spawns.size() - 1)}),
                     "Baseline fire commands were rejected");
        for (std::uint64_t tick = 0; tick <= 12800; tick += 8) {
            if (tick == 128) world.enqueue({middle});
            if (tick != 0) world.update(tick);
            if (tick == 0 || tick == 8 || tick == 32 || tick == 128 || tick == 640
                || tick == 2048 || tick == 6400 || tick == 12800) {
                std::uint32_t expected_seed;
                std::uint64_t expected_tick, expected_digest;
                test::expect(static_cast<bool>(fixture >> expected_seed >> expected_tick >> expected_digest),
                             "Missing replay checkpoint");
                test::expect(seed == expected_seed && tick == expected_tick, "Replay fixture order changed");
                test::expect(digest(world) == expected_digest,
                    "Runtime state differs at seed " + std::to_string(seed) + " tick " + std::to_string(tick));
            }
        }
    }
    std::string trailing;
    test::expect(!(fixture >> trailing), "Unconsumed replay checkpoints");
}

struct MovedTable {};

void check_handles_and_queries()
{
    TileMap map{12, 12, 42, std::vector<Terrain>(144, Terrain::grass), {}};
    World world;
    world.reset(map, {{{0, 0}, 2, {0.1f, -0.1f}}});
    const auto original = world.actor_at(map.index({0, 0}));
    const auto snapshot = world.inspect(original);
    test::expect(snapshot && snapshot->id == 1 && snapshot->selectable, "Tree factory missed shared components");
    auto entity = world.registry().entity(original.entity);
    test::expect(entity.has<ActorIdentity>() && entity.has<ActorPosition>()
                 && entity.has<ActorAppearance>() && entity.has<Tree>() && entity.has<Selectable>(),
                 "Tree is not stored as Flecs components");
    entity.add<MovedTable>();
    test::expect(world.inspect(original)->id == snapshot->id && world.actors().size() == 1,
                 "Moving an actor between ECS tables invalidated selection/query");
    entity.get_mut<Tree>().dead_ticks += 8;
    test::expect(world.inspect(original)->tree->dead_ticks == snapshot->tree->dead_ticks + 8,
                 "Inspection is not reading authoritative ECS state");
    auto copy = world.actors();
    copy.front().position.hex = {99, 99};
    test::expect(world.inspect(original)->position.hex == glm::ivec2(0), "A view mutated the world");

    test::expect(world.destroy(original) && !world.alive(original), "Deleted handle remains alive");
    const auto replacement = world.spawn_tree({{0, 0}, 0, {}}, Tree{});
    test::expect(replacement != original && !world.inspect(original) && !world.enqueue({original}),
                 "Deleted/reused entity accepted an old handle");
    test::expect(world.inspect(replacement)->id > snapshot->id, "Gameplay ID was recycled");
    world.reset(map, {{{0, 0}, 2, {}}});
    test::expect(!world.alive(replacement) && !world.enqueue({replacement}),
                 "Regeneration accepted an old-world handle");
    test::expect(world.actor_at(-1) == ActorHandle{} && !world.actor_at(144), "Outside occupancy is not empty");
}

void check_pause_and_reset()
{
    Simulation simulation;
    simulation.island_settings.seed = 74129;
    simulation.regenerate();
    const auto actor = simulation.world().actors().front().handle;
    simulation.settings.speed = 0;
    test::expect(simulation.enqueue({actor}), "Paused command was rejected");
    simulation.advance_simulation(30);
    test::expect(simulation.clock().ticks() == 0 && !simulation.world().inspect(actor)->tree->burning(),
                 "Paused Flecs systems progressed");
    simulation.settings.speed = 50;
    simulation.advance_simulation(0.25);
    test::expect(simulation.clock().ticks() == 400 && simulation.pending_ignitions() == 0,
                 "Fixed-step Flecs scheduling did not consume 50x time");
    simulation.regenerate();
    test::expect(!simulation.world().inspect(actor) && !simulation.enqueue({actor})
                 && simulation.clock().ticks() == 0, "Reset retained actor handles or elapsed time");
}

} // namespace

int main()
{
    return test::run("Flecs actor world", [] {
        check_runtime_replay();
        check_handles_and_queries();
        check_pause_and_reset();
    });
}
