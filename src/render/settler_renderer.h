#pragma once
#include "aseprite_texture.h"
#include "settler_layout.h"
#include <unordered_map>

class SettlerRenderer {
public:
    struct Entry { SettlerLayout layout; ActorId id; std::size_t frame; bool flipped; };
    bool initialize(TextureCache& textures);
    void shutdown();
    void prepare(const std::vector<ActorView>& actors, const ActorSceneView& view,
                 std::uint64_t tick, std::uint64_t generation);
    void draw(SDL_Renderer* renderer, const Entry& entry) const;
    std::optional<ActorSceneEntry> hit(const ActorView& actor, const ActorSceneView& view, glm::vec2 mouse) const;
    const std::vector<Entry>& entries() const { return entries_; }
    const CachedAnimation* animation() const { return animation_; }
private:
    struct State {
        ActorPosition position;
        std::string clip;
        std::uint64_t started_ms = 0;
        std::uint64_t random = 1;
        bool flipped = false;
        bool dead = false;
        bool seen = false;
    };
    const CachedAnimation* animation_ = nullptr; // Borrowed from application cache.
    std::unordered_map<ActorId, State> states_;
    std::vector<Entry> entries_;
    std::uint64_t generation_ = 0;
    std::uint64_t last_tick_ = 0;
    void choose_idle(State& state);
};
