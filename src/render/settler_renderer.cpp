#include "settler_renderer.h"

bool SettlerRenderer::initialize(TextureCache& textures)
{
    shutdown();
    animation_ = textures.load_animation("minifolks/villagers2/MiniLumberjack.aseprite");
    if (!animation_) { SDL_Log("Cannot load settler sprite: %s", textures.error().c_str()); return false; }
    for (const char* name : {"idle", "idle2", "walk", "work", "death"}) {
        if (!animation_->clips.count(name)) {
            SDL_Log("Settler sprite is missing animation %s", name); shutdown(); return false;
        }
    }
    return true;
}
void SettlerRenderer::shutdown()
{
    animation_ = nullptr; states_.clear(); entries_.clear(); generation_ = last_tick_ = 0;
}
void SettlerRenderer::choose_idle(State& state)
{
    state.random ^= state.random << 13;
    state.random ^= state.random >> 7;
    state.random ^= state.random << 17;
    state.clip = state.random & 1 ? "idle" : "idle2";
}
void SettlerRenderer::prepare(const std::vector<ActorView>& actors, const ActorSceneView& view,
                              std::uint64_t tick, std::uint64_t generation)
{
    entries_.clear();
    if (!animation_) return;
    if (generation != generation_ || tick < last_tick_) states_.clear();
    generation_ = generation; last_tick_ = tick;
    const std::uint64_t now_ms = tick * 1000 / 32;
    for (auto& pair : states_) pair.second.seen = false;
    for (const auto& actor : actors) {
        if (!actor.settler) continue;
        auto [it, inserted] = states_.try_emplace(actor.id);
        auto& state = it->second;
        if (inserted) { state.random = actor.id + 1; state.position = actor.position; }
        const auto previous = hex_center(state.position.hex) + state.position.offset;
        const auto ground = hex_center(actor.position.hex) + actor.position.offset;
        if (ground.x != previous.x) state.flipped = ground.x < previous.x;
        state.position = actor.position;
        state.seen = true;
        const auto& settler = *actor.settler;
        const bool walking = settler.action != SettlerAction::idle && settler.waypoint < settler.route.size();
        const char* desired = walking ? "walk" : settler.action == SettlerAction::cut ? "work" : "idle";
        const bool idle = state.clip == "idle" || state.clip == "idle2";
        if (state.clip.empty() || (std::string(desired) == "idle" ? !idle : state.clip != desired)) {
            state.started_ms = now_ms;
            if (std::string(desired) == "idle") choose_idle(state); else state.clip = desired;
        }
    }
    for (auto it = states_.begin(); it != states_.end();) {
        auto& state = it->second;
        if (!state.seen && !state.dead) { state.dead = true; state.clip = "death"; state.started_ms = now_ms; }
        auto elapsed = now_ms - state.started_ms;
        if (state.dead && elapsed >= animation_->clips.at("death").duration_ms) { it = states_.erase(it); continue; }
        while ((state.clip == "idle" || state.clip == "idle2") && elapsed >= animation_->clips.at(state.clip).duration_ms) {
            state.started_ms += animation_->clips.at(state.clip).duration_ms;
            choose_idle(state);
            elapsed = now_ms - state.started_ms;
        }
        const auto frame = animation_->frame_at(animation_->clips.at(state.clip), elapsed);
        entries_.push_back({settler_layout(state.position, view), it->first, frame, state.flipped});
        ++it;
    }
}
void SettlerRenderer::draw(SDL_Renderer* renderer, const Entry& entry) const
{
    const auto& p = entry.layout.minimum;
    const auto& size = entry.layout.size;
    // SDL flips about the canvas center, so compensate its asymmetric foot anchor.
    const float x = entry.flipped ? entry.layout.ground.x - size.x * (18.0f / 32) : p.x;
    const SDL_FRect destination{x, p.y, size.x, size.y};
    SDL_RenderTextureRotated(renderer, animation_->frames[entry.frame]->texture, nullptr, &destination,
                             0, nullptr, entry.flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}
