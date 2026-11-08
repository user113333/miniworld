#pragma once

#include "test_state.h"
#include "simulation/simulation.h"
#include "simulation/tuning.h"
#include <algorithm>
#include <unordered_set>

namespace test {

inline void publish(const World& world, MapState& state)
{
    const auto actors = world.actors();
    std::unordered_map<ActorId, std::size_t> by_id;
    std::unordered_map<int, std::size_t> by_cell;
    for (std::size_t i = 0; i < actors.size(); ++i) {
        by_id.emplace(actors[i].id, i);
        by_cell.emplace(world.map().index(actors[i].position.hex), i);
    }
    std::vector<bool> published(actors.size(), false);
    std::vector<TreeRecord> records;
    // Retain fixture order without depending on Flecs table iteration order.
    for (const auto& previous : state.trees) {
        const auto found = previous.id ? by_id.find(previous.id) : by_id.end();
        const auto cell = by_cell.find(state.index(previous.hex));
        const auto index = found != by_id.end() ? found->second
            : !previous.id && cell != by_cell.end() ? cell->second : actors.size();
        if (index < actors.size() && !published[index]) {
            records.push_back(record(actors[index]));
            published[index] = true;
        }
    }
    for (std::size_t i = 0; i < actors.size(); ++i)
        if (!published[i] && actors[i].tree) records.push_back(record(actors[i]));
    static_cast<TileMap&>(state) = world.map();
    state.trees = std::move(records);
}

// Runs the pre-existing behavior assertions against real Flecs systems. Tests may
// edit fixture values between steps; those edits are applied to ECS components.
class WorldFixture {
public:
    World world;

    void reset(MapState& map)
    {
        std::vector<TreeSpawn> spawns;
        for (const auto& tree : map.trees) spawns.push_back({tree.hex, tree.variant, tree.offset});
        world.reset(map, spawns);
        state_ = &map;
        publish(world, map);
    }

    void update(MapState& map, std::uint64_t tick)
    {
        synchronize(map);
        world.update(tick);
        state_ = &map;
        publish(world, map);
    }

    bool queue_ignition(const MapState& map, ActorId id)
    {
        world.map() = map;
        const auto target = world.handle(id);
        const auto tree = std::find_if(map.trees.begin(), map.trees.end(),
            [id](const TreeRecord& candidate) { return candidate.id == id; });
        if (!target || tree == map.trees.end()) return false;
        world.registry().entity(target.entity).set<Tree>(static_cast<const Tree&>(*tree));
        return world.enqueue({target});
    }
    std::size_t pending_ignitions() const { return world.pending_ignitions(); }
    int tree_index_at(int cell) const
    {
        const auto actor = world.inspect(world.actor_at(cell));
        if (!state_ || !actor) return -1;
        for (std::size_t i = 0; i < state_->trees.size(); ++i)
            if (state_->trees[i].id == actor->id) return static_cast<int>(i);
        return -1;
    }

private:
    MapState* state_ = nullptr;

    void synchronize(const MapState& map)
    {
        world.map() = map;
        std::unordered_set<ActorId> present;
        for (const auto& tree : map.trees) present.insert(tree.id);
        for (const auto& actor : world.actors())
            if (!present.count(actor.id)) world.destroy(actor.handle);
        for (const auto& tree : map.trees) {
            const auto handle = world.handle(tree.id);
            if (!handle) continue;
            world.registry().entity(handle.entity)
                .set<ActorPosition>({tree.hex, tree.offset})
                .set<ActorAppearance>({ActorKind::tree, tree.variant})
                .set<Tree>(static_cast<const Tree&>(tree));
        }
        world.rebuild_occupancy();
        for (const auto& tree : map.trees) {
            if (!world.handle(tree.id)) world.spawn_tree({tree.hex, tree.variant, tree.offset}, tree);
        }
    }
};

struct FixtureSettings : IslandSettings {
    float simulation_speed = 1.0f;
    bool select_entities = false;
    bool ignite_trees = false;
};

class SimulationFixture {
public:
    SimulationFixture() { copy_settings(); publish(simulation_.world(), state_); }
    FixtureSettings settings;
    void generate() { apply_settings(); simulation_.generate(); reset_view(); }
    void regenerate() { apply_settings(); simulation_.regenerate(); reset_view(); }
    void advance_simulation(double seconds, bool active = true)
    { apply_settings(); simulation_.advance_simulation(seconds, active); publish(simulation_.world(), state_); }
    void accumulate_simulation_time(double seconds, bool active = true)
    { apply_settings(); simulation_.accumulate_simulation_time(seconds, active); }
    bool step_simulation()
    {
        const bool advanced = simulation_.step_simulation();
        if (advanced && simulation_.clock().world_update_due()) publish(simulation_.world(), state_);
        return advanced;
    }
    bool queue_ignition(ActorId id, std::uint64_t generation)
    { return generation == world_generation() && simulation_.enqueue({simulation_.world().handle(id)}); }
    std::size_t pending_ignitions() const { return simulation_.pending_ignitions(); }
    void toggle_pause()
    { apply_settings(); simulation_.toggle_pause(); settings.simulation_speed = simulation_.settings.speed; }
    const MapState& map() const { return state_; }
    const SimulationClock& clock() const { return simulation_.clock(); }
    std::uint64_t world_generation() const { return simulation_.world_generation(); }

private:
    Simulation simulation_;
    MapState state_{0, 0, 0, {}};
    void apply_settings()
    {
        simulation_.island_settings = settings;
        simulation_.settings.speed = settings.simulation_speed;
    }
    void copy_settings()
    {
        static_cast<IslandSettings&>(settings) = simulation_.island_settings;
        settings.simulation_speed = simulation_.settings.speed;
    }
    void reset_view()
    {
        settings.select_entities = settings.ignite_trees = false;
        copy_settings();
        state_.trees.clear();
        publish(simulation_.world(), state_);
    }
};

} // namespace test
