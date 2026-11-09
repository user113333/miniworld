#pragma once

#include "actors/actor_view.h"
#include "commands.h"
#include "world/tile_map.h"
#include <flecs.h>
#include <memory>
#include <unordered_map>
#include <vector>

class World {
public:
    World();
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void reset(TileMap map, const std::vector<TreeSpawn>& trees);
    void update(std::uint64_t tick);
    bool enqueue(IgniteActor command);
    std::size_t pending_ignitions() const { return pending_ignitions_.size(); }

    TileMap& map() { return map_; }
    const TileMap& map() const { return map_; }
    flecs::world& registry();
    const flecs::world& registry() const;
    std::uint64_t generation() const { return generation_; }
    bool alive(ActorHandle handle) const;
    ActorHandle handle(ActorId id) const;
    ActorHandle actor_at(int cell) const;
    std::optional<ActorView> inspect(ActorHandle handle) const;
    std::vector<ActorView> actors() const;

    ActorHandle spawn_tree(const TreeSpawn& spawn, const Tree& state);
    bool destroy(ActorHandle handle);
    void rebuild_occupancy();

private:
    struct Runtime;
    std::unique_ptr<Runtime> runtime_;
    TileMap map_{};
    std::uint64_t generation_ = 0;
    std::uint64_t tick_ = 0;
    ActorId next_actor_id_ = 1;
    std::unordered_map<ActorId, flecs::entity_t> identities_;
    std::vector<flecs::entity_t> occupancy_;
    std::vector<int> ignitions_;
    std::vector<IgniteActor> pending_ignitions_;
    std::vector<ActorHandle> decayed_;

    void prepare_step();
    void collect_ignitions();
    void advance_recovery();
    void apply_ignitions();
    void advance_tree(flecs::entity entity, const ActorIdentity& identity,
                       const ActorPosition& position, Tree& tree);
    void commit_actors();
    void burn_down(const ActorIdentity& identity, const ActorPosition& position, Tree& tree);
};
