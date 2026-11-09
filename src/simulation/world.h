#pragma once

#include "actors/actor_view.h"
#include "commands.h"
#include "entity_inspection.h"
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
    void step_settlers(std::uint64_t tick);
    void populate_settlers();
    ActorHandle spawn_settler(glm::ivec2 hex);
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

    bool entity_alive(EntityHandle handle) const;
    std::vector<EntitySummary> entities() const;
    std::optional<EntityInspection> inspect_entity(EntityHandle handle) const;
    std::uint64_t submit_edit(EntityEdit edit);
    void flush_edits();
    std::optional<EntityEditResult> take_edit_result(std::uint64_t request);

    ActorHandle spawn_tree(const TreeSpawn& spawn, const Tree& state);
    bool destroy(ActorHandle handle);
    void rebuild_occupancy();

private:
    struct PendingEdit { std::uint64_t request; EntityEdit edit; };
    std::vector<PendingEdit> edits_;
    std::vector<EntityEditResult> edit_results_;
    std::uint64_t next_edit_request_ = 1;
    EntityEditResult apply_edit(const PendingEdit& pending);

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

    std::uint64_t settler_tick_ = 0;
    void advance_growth();
    void advance_settlers();
    void advance_settler(ActorPosition& position, Settler& settler);
    void choose_action(const ActorPosition& position, Settler& settler);
    void prepare_step();
    void collect_ignitions();
    void advance_recovery();
    void apply_ignitions();
    void advance_tree(flecs::entity entity, const ActorIdentity& identity,
                       const ActorPosition& position, Tree& tree);
    void commit_actors();
    void burn_down(const ActorIdentity& identity, const ActorPosition& position, Tree& tree);
};
