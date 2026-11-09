#include "world.h"
#include "actors/reflection.h"
#include "systems/random.h"

#include <algorithm>
#include <array>
#include <stdexcept>

using namespace simulation_detail;

struct World::Runtime {
    flecs::world ecs;
    flecs::query<const ActorIdentity, const ActorPosition, const ActorAppearance> actors;
    std::array<flecs::system, 6> systems;

    explicit Runtime(World& owner)
        : actors(ecs.query<const ActorIdentity, const ActorPosition, const ActorAppearance>())
    {
        register_actor_reflection(ecs);
        systems[0] = ecs.system<>("PrepareWorldStep").kind(0)
            .run([&owner](flecs::iter&) { owner.prepare_step(); });
        systems[1] = ecs.system<>("RecoverTerrain").kind(0)
            .run([&owner](flecs::iter&) { owner.advance_recovery(); });
        systems[2] = ecs.system<>("ApplyIgnitions").kind(0)
            .run([&owner](flecs::iter&) { owner.apply_ignitions(); });
        systems[3] = ecs.system<const ActorIdentity, const ActorPosition, Tree>("TreeLifecycle").kind(0)
            .each([&owner](flecs::entity entity, const ActorIdentity& id,
                           const ActorPosition& position, Tree& tree) {
                owner.advance_tree(entity, id, position, tree);
            });
        systems[4] = ecs.system<>("CommitActors").kind(0)
            .run([&owner](flecs::iter&) { owner.commit_actors(); });
        systems[5] = ecs.system<>("RefreshOccupancy").kind(0)
            .run([&owner](flecs::iter&) { owner.rebuild_occupancy(); });
    }
};

World::World() : runtime_(std::make_unique<Runtime>(*this)) {}
World::~World() = default;
flecs::world& World::registry() { return runtime_->ecs; }
const flecs::world& World::registry() const { return runtime_->ecs; }

void World::reset(TileMap map, const std::vector<TreeSpawn>& trees)
{
    edits_.clear();
    edit_results_.clear();
    map_ = std::move(map);
    for (std::size_t cell = 0; cell < map_.tiles.size() && cell < map_.recovery.size(); ++cell) {
        const auto& recovery = map_.recovery[cell];
        if (map_.tiles[cell] == Terrain::burned && recovery.duration_ticks != 0) {
            map_.tiles[cell] = recovery.original_terrain;
        }
    }
    map_.recovery.assign(map_.tiles.size(), {});
    // Destroy queries/systems with their old registry before constructing a new one.
    runtime_.reset();
    runtime_ = std::make_unique<Runtime>(*this);
    ++generation_;
    tick_ = 0;
    next_actor_id_ = 1;
    identities_.clear();
    occupancy_.assign(map_.tiles.size(), 0);
    ignitions_.clear();
    pending_ignitions_.clear();
    decayed_.clear();

    auto ordered = trees;
    for (const auto& spawn : ordered) {
        if (!map_.contains(spawn.hex)) throw std::invalid_argument("Tree lies outside its world");
    }
    std::sort(ordered.begin(), ordered.end(), [this](const TreeSpawn& a, const TreeSpawn& b) {
        return map_.index(a.hex) < map_.index(b.hex);
    });
    for (const auto& spawn : ordered) {
        spawn_tree(spawn, Tree{});
    }
    rebuild_occupancy();
}

bool World::alive(ActorHandle handle) const
{
    return handle && handle.generation == generation_ && registry().is_alive(handle.entity)
           && registry().entity(handle.entity).has<ActorIdentity>();
}

ActorHandle World::handle(ActorId id) const
{
    const auto found = identities_.find(id);
    const ActorHandle result{found == identities_.end() ? 0 : found->second, generation_};
    return alive(result) ? result : ActorHandle{};
}

ActorHandle World::actor_at(int cell) const
{
    if (cell < 0 || static_cast<std::size_t>(cell) >= occupancy_.size()) return {};
    const ActorHandle result{occupancy_[cell], generation_};
    return alive(result) ? result : ActorHandle{};
}

std::optional<ActorView> World::inspect(ActorHandle handle) const
{
    if (!alive(handle)) return std::nullopt;
    const auto entity = registry().entity(handle.entity);
    const auto* position = entity.try_get<ActorPosition>();
    const auto* appearance = entity.try_get<ActorAppearance>();
    if (!position || !appearance) return std::nullopt;
    ActorView view{handle, entity.get<ActorIdentity>().value, *position, *appearance,
                   entity.has<Selectable>(), std::nullopt};
    if (const auto* tree = entity.try_get<Tree>()) view.tree = *tree;
    return view;
}

std::vector<ActorView> World::actors() const
{
    std::vector<ActorView> views;
    runtime_->actors.each([&](flecs::entity entity, const ActorIdentity& id,
                              const ActorPosition& position, const ActorAppearance& appearance) {
        ActorView view{{entity.id(), generation_}, id.value, position, appearance,
                       entity.has<Selectable>(), std::nullopt};
        if (const auto* tree = entity.try_get<Tree>()) view.tree = *tree;
        views.push_back(view);
    });
    std::sort(views.begin(), views.end(), [](const ActorView& a, const ActorView& b) { return a.id < b.id; });
    return views;
}

ActorHandle World::spawn_tree(const TreeSpawn& spawn, const Tree& state)
{
    if (!map_.contains(spawn.hex)) throw std::invalid_argument("Tree lies outside its world");
    const int cell = map_.index(spawn.hex);
    if (occupancy_[cell] != 0) throw std::invalid_argument("Multiple trees occupy one cell");
    const auto id = next_actor_id_++;
    const auto entity = create_tree(registry(), id, spawn, state);
    identities_[id] = entity.id();
    occupancy_[cell] = entity.id();
    return {entity.id(), generation_};
}

bool World::destroy(ActorHandle handle)
{
    if (!alive(handle)) return false;
    const auto entity = registry().entity(handle.entity);
    identities_.erase(entity.get<ActorIdentity>().value);
    if (const auto* position = entity.try_get<ActorPosition>(); position && map_.contains(position->hex)) {
        auto& occupant = occupancy_[map_.index(position->hex)];
        if (occupant == entity.id()) occupant = 0;
    }
    entity.destruct();
    return true;
}

void World::rebuild_occupancy()
{
    occupancy_.assign(map_.tiles.size(), 0);
    identities_.clear();
    runtime_->actors.each([&](flecs::entity entity, const ActorIdentity& id,
                              const ActorPosition& position, const ActorAppearance&) {
        if (!id.value || !identities_.emplace(id.value, entity.id()).second)
            throw std::invalid_argument("Actor ID is zero or duplicated");
        next_actor_id_ = std::max(next_actor_id_, id.value + 1);
        if (!map_.contains(position.hex)) throw std::invalid_argument("Actor lies outside its world");
        // Trees block forest occupancy. Moving actors will use their own spatial rules.
        if (entity.has<Tree>()) {
            auto& occupant = occupancy_[map_.index(position.hex)];
            if (occupant != 0) throw std::invalid_argument("Multiple trees occupy one cell");
            occupant = entity.id();
        }
    });
}

void World::prepare_step()
{
    rebuild_occupancy();
    collect_ignitions();
    decayed_.clear();
}

void World::commit_actors()
{
    for (auto handle : decayed_) destroy(handle);
}

void World::update(std::uint64_t tick)
{
    if (tick != tick_ + world_ticks)
        throw std::invalid_argument("World updates require successive eight-tick boundaries");
    flush_edits();
    tick_ = tick;
    // Manual Flecs systems have explicit merge boundaries. Intents were collected
    // before any changes, preserving fire/recovery semantics across tables.
    for (const auto& system : runtime_->systems) system.run(world_seconds);
}
