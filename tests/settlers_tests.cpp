#include "test_support.h"
#include "simulation/simulation.h"
#include "simulation/tuning.h"
#include "world/navigation.h"
#include <set>

namespace {
struct Fixture {
    World world;
    std::uint64_t tick = 0;
    Fixture(int size = 15) {
        TileMap map{size, size, 74129, std::vector<Terrain>(size * size, Terrain::grass), {}};
        world.reset(std::move(map), {});
    }
    void step(int count = 1) {
        while (count--) {
            ++tick;
            if (tick % 8 == 0) world.update(tick);
            world.step_settlers(tick);
        }
    }
    Settler state(ActorHandle h) { return world.registry().entity(h.entity).get<Settler>(); }
    void set(ActorHandle h, Settler s) { world.registry().entity(h.entity).set<Settler>(s); }
};
void inventory()
{
    Inventory items;
    test::expect(items.add(ItemType::seeds, 75) == 75, "fill seed stacks");
    test::expect(items.add(ItemType::logs, 4) == 0, "no mixed slot or overflow slot");
    test::expect(items.add(ItemType::seeds, 20) == 5, "discard excess");
    for (const auto& slot : items.slots) test::expect(slot.quantity == 10, "slot capacity");
    for (int i = 0; i < 10; ++i) test::expect(items.consume(ItemType::seeds), "consume seeds");
    test::expect(items.slots[0].type == ItemType::empty, "empty slot clears type");
    test::expect(items.add(ItemType::logs, 12) == 10, "reuse empty slot");
    test::expect(!items.consume(ItemType::empty), "cannot consume empty inventory");
}
void navigation()
{
    Fixture f;
    f.world.map().tiles[f.world.map().index({1, 0})] = Terrain::water;
    auto route = find_route(f.world.map(), {0, 0}, {2, 0});
    test::expect(route.size() == 4, "A* shortest detour");
    for (std::size_t i = 0; i < route.size(); ++i) {
        test::expect(walkable(f.world.map(), route[i]), "route crosses water");
        if (i) test::expect(hex_distance(route[i-1], route[i]) == 1, "nonadjacent route edge");
    }
    test::expect(find_route(f.world.map(), {0, 0}, {2, 0}, 1).empty(), "bounded radius");
    for (auto delta : hex_neighbors) f.world.map().tiles[f.world.map().index(delta)] = Terrain::water;
    test::expect(find_route(f.world.map(), {0, 0}, {2, 0}).empty(), "unreachable island");
    f.world.map().tiles[f.world.map().index({1, 0})] = static_cast<Terrain>(99);
    test::expect(!walkable(f.world.map(), {1, 0}), "unknown future terrain blocks navigation");

    Fixture moving;
    moving.world.map().tiles[moving.world.map().index({1, 0})] = Terrain::water;
    auto h = moving.world.spawn_settler({0, 0});
    auto s = moving.state(h);
    s.action = SettlerAction::wander;
    s.destination = {2, 0};
    s.route = find_route(moving.world.map(), {0, 0}, s.destination);
    moving.set(h, s);
    glm::vec2 previous{};
    bool intermediate = false;
    for (int i = 0; i < 100; ++i) {
        moving.step();
        const auto view = *moving.world.inspect(h);
        const auto p = hex_center(view.position.hex) + view.position.offset;
        test::expect(glm::length(p - previous) <= s.dna.speed / 32 + 0.00001f, "movement snaps or exceeds speed");
        test::expect(walkable(moving.world.map(), world_hex(p)), "movement crossed water");
        intermediate |= glm::length(view.position.offset) > 0.05f;
        previous = p;
    }
    test::expect(intermediate, "movement has no continuous offsets");
    auto interrupted = moving.state(h);
    interrupted.route = {{3, 0}};
    interrupted.waypoint = 0;
    interrupted.action = SettlerAction::wander;
    moving.set(h, interrupted);
    moving.world.map().tiles[moving.world.map().index({3, 0})] = Terrain::water;
    moving.step();
    test::expect(moving.state(h).action == SettlerAction::idle, "blocked route not cancelled");
}
void work()
{
    Fixture f;
    const auto tree = f.world.spawn_tree({{0, 0}, 0, {}}, {});
    const auto a = f.world.spawn_settler({0, 0});
    const auto b = f.world.spawn_settler({0, 0});
    test::expect(f.world.actor_at(f.world.map().index({0, 0})) == tree, "settlers overwrite tree occupancy");
    for (auto h : {a, b}) {
        auto s = f.state(h);
        s.action = SettlerAction::cut;
        s.target = tree;
        s.destination = {0, 0};
        s.remaining_ticks = 96;
        f.set(h, s);
    }
    f.step(95);
    test::expect(f.world.alive(tree), "cut finished early");
    f.step();
    test::expect(!f.world.alive(tree), "tree not harvested after three seconds");
    test::expect(f.state(a).inventory.count(ItemType::seeds) + f.state(b).inventory.count(ItemType::seeds) == 2,
                 "double seed yield");
    test::expect(f.state(a).inventory.count(ItemType::logs) + f.state(b).inventory.count(ItemType::logs) == 4,
                 "double log yield");
    for (auto h : {a, b}) {
        auto s = f.state(h);
        s.inventory.add(ItemType::seeds, 1);
        s.action = SettlerAction::plant;
        s.remaining_ticks = 64;
        s.destination = {0, 0};
        s.route.clear();
        f.set(h, s);
    }
    const int before = f.state(a).inventory.count(ItemType::seeds) + f.state(b).inventory.count(ItemType::seeds);
    f.step(63);
    test::expect(!f.world.actor_at(f.world.map().index({0, 0})), "plant finished early");
    f.step();
    const auto planted = f.world.actor_at(f.world.map().index({0, 0}));
    test::expect(planted && !f.world.inspect(planted)->tree->fully_grown(), "planted tree starts mature");
    test::expect(f.state(a).inventory.count(ItemType::seeds) + f.state(b).inventory.count(ItemType::seeds) == before - 1,
                 "failed planting consumed seed");
    // Keep the workers inactive while checking the exact growth boundary.
    for (auto h : {a, b}) { auto s = f.state(h); s.action = SettlerAction::idle; s.remaining_ticks = 10000; f.set(h, s); }
    f.step(1919);
    test::expect(!f.world.inspect(planted)->tree->fully_grown(), "tree grew before sixty seconds");
    f.step();
    test::expect(f.world.inspect(planted)->tree->fully_grown(), "tree did not mature at sixty seconds");

    auto s = f.state(a);
    s.action = SettlerAction::cut; s.target = planted; s.destination = {0, 0}; s.remaining_ticks = 96;
    f.set(a, s);
    test::expect(f.world.enqueue({planted}), "ignition failed");
    f.step(8);
    test::expect(f.state(a).action == SettlerAction::idle, "burning target did not cancel cut");
    test::expect(f.state(a).inventory.count(ItemType::logs) == s.inventory.count(ItemType::logs), "cancelled cut yields logs");

    // A plant completed between 4 Hz boundaries must still get sixty full seconds.
    Fixture off_boundary;
    const auto planter = off_boundary.world.spawn_settler({});
    auto planting = off_boundary.state(planter);
    planting.action = SettlerAction::plant;
    planting.inventory.add(ItemType::seeds, 1);
    planting.remaining_ticks = 1;
    off_boundary.set(planter, planting);
    off_boundary.step();
    const auto sapling = off_boundary.world.actor_at(off_boundary.world.map().index({}));
    auto resting = off_boundary.state(planter);
    resting.remaining_ticks = 10000;
    off_boundary.set(planter, resting);
    off_boundary.step(1919);
    test::expect(!off_boundary.world.inspect(sapling)->tree->fully_grown(), "off-boundary growth finished early");
    off_boundary.step();
    test::expect(off_boundary.world.inspect(sapling)->tree->fully_grown(), "off-boundary growth finished late");

    // Full inventory never prevents harvesting and receives no duplicate yield.
    Fixture full;
    auto t = full.world.spawn_tree({{}, 0, {}}, {});
    auto worker = full.world.spawn_settler({});
    auto state = full.state(worker);
    state.inventory.add(ItemType::logs, 79);
    state.action = SettlerAction::cut; state.target = t; state.remaining_ticks = 96;
    full.set(worker, state);
    full.step(96);
    test::expect(!full.world.alive(t) && full.state(worker).inventory.count(ItemType::logs) == 80,
                 "overflow must discard yield without stopping harvest");
}
void thoughts()
{
    Fixture f;
    auto h = f.world.spawn_settler({});
    auto s = f.state(h);
    s.next_thought = 32; s.remaining_ticks = 10000;
    f.set(h, s);
    f.step(31);
    test::expect(f.state(h).happiness == 500, "continuous happiness adjustment");
    f.step();
    test::expect(f.state(h).happiness == 510 && f.state(h).thought_ticks == 320, "thought does not apply immediately");
    test::expect(f.state(h).next_thought >= f.tick + 320 && f.state(h).next_thought <= f.tick + 960, "thought schedule outside bounds");
    s = f.state(h); s.next_thought = 10000; f.set(h, s);
    f.step(319);
    test::expect(!f.state(h).thought.empty(), "thought expired early");
    f.step();
    test::expect(f.state(h).thought.empty(), "thought did not disappear after ten seconds");
    s = f.state(h); s.happiness = 999; s.next_thought = f.tick + 1; f.set(h, s);
    f.step();
    test::expect(f.state(h).happiness == 1000, "happiness cap");
    for (int i = 0; i < 8; ++i) f.world.spawn_settler({});
    s = f.state(h); s.happiness = 1; s.inventory.add(ItemType::logs, 20); s.next_thought = f.tick + 1; f.set(h, s);
    f.step();
    test::expect(!f.world.alive(h), "zero happiness must delete actor with inventory");
    test::expect(!f.world.actor_at(f.world.map().index({})), "death leaves occupancy or drops");

    Fixture trees;
    auto person = trees.world.spawn_settler({});
    int planted = 0;
    for (int q = -2; q <= 2; ++q) for (int r = -2; r <= 2; ++r)
        if (planted < 13 && hex_distance({}, {q, r}) <= 2) {
            trees.world.spawn_tree({{q, r}, 0, {}}, {}); ++planted;
        }
    auto state = trees.state(person); state.next_thought = 1; state.remaining_ticks = 10000; trees.set(person, state);
    trees.step();
    test::expect(trees.state(person).happiness < 500, "excess trees should lower happiness");
}
void generation_and_pacing()
{
    Simulation a, b;
    a.island_settings.seed = b.island_settings.seed = 74129;
    a.regenerate(); b.regenerate();
    std::vector<ActorHandle> handles;
    std::set<int> cells;
    for (const auto& view : a.world().actors()) if (view.settler) {
        const auto& s = *view.settler;
        handles.push_back(view.handle);
        cells.insert(a.map().index(view.position.hex));
        test::expect(s.happiness == 500 && s.inventory.count(ItemType::seeds) == 0 && s.inventory.count(ItemType::logs) == 0, "initial state");
        test::expect(s.dna.speed >= .5f && s.dna.speed <= 1.5f, "DNA speed bounds");
        test::expect(s.dna.people_sensitivity >= .5f && s.dna.people_sensitivity <= 1.5f, "DNA sensitivity bounds");
        const auto other = b.world().inspect(b.world().handle(view.id));
        test::expect(other && other->position.hex == view.position.hex && other->settler->random_state == s.random_state, "generation not deterministic");
    }
    test::expect(handles.size() == 3 && cells.size() == 3, "separate three settlers");
    a.settings.speed = 0;
    a.advance_simulation(.25);
    test::expect(a.clock().ticks() == 0 && a.world().inspect(handles[0])->settler->idle_ticks == 0, "pause advances settlers");
    a.settings.speed = 50;
    a.advance_simulation(.25, false);
    test::expect(a.clock().ticks() == 0, "inactive simulation advances settlers");
    a.advance_simulation(.25);
    test::expect(a.clock().ticks() == 400, "shared speed clock");
    a.regenerate();
    for (auto h : handles) test::expect(!a.world().alive(h), "regeneration retains handle");
    test::expect(a.clock().ticks() == 0, "regeneration retains settler time");

    // A single autonomous settler on open terrain survives long enough to measure
    // elapsed activity duration without crowd deaths biasing the sample.
    std::uint64_t idle = 0, active = 0;
    for (int seed = 0; seed < 4; ++seed) {
        Fixture f(25);
        f.world.map().seed = seed;
        auto h = f.world.spawn_settler({});
        for (int i = 0; i < 15; ++i) f.world.spawn_tree({f.world.map().hex_at(210 + i), 0, {}}, {});
        bool wandered = false, cut = false, planted = false;
        for (int second = 0; second < 3600; ++second) {
            f.step(32);
            const auto live = f.state(h);
            wandered |= live.action == SettlerAction::wander;
            cut |= live.action == SettlerAction::cut;
            planted |= live.action == SettlerAction::plant;
        }
        test::expect(wandered && cut && planted, "autonomous selector did not use all available activities");
        test::expect(f.world.alive(h), "uncrowded settler died");
        const auto s = f.state(h);
        idle += s.idle_ticks; active += s.active_ticks;
    }
    const double fraction = static_cast<double>(idle) / (idle + active);
    std::cout << "elapsed idle fraction: " << fraction << '\n';
    test::expect(fraction > .74 && fraction < .86, "AI must idle approximately 80% of elapsed time");
}
}
int main() { return test::run("settlers", [] {
    inventory(); navigation(); work(); thoughts(); generation_and_pacing();
}); }
