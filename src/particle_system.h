#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

enum class ParticleKind { flame, smoke, ember };

struct FireEmitter {
    std::uint64_t id = 0;
    glm::vec2 ground{};
    float height = 1.5f; // World/hex-radius units, independent of screen scale.
    std::uint64_t ignition_tick = 0;
    std::uint64_t burnout_tick = 0;
};

struct Particle {
    glm::vec2 position{};
    glm::vec2 velocity{};
    glm::vec2 ground{}; // Stored depth anchor also survives emitter removal.
    glm::vec4 color{};
    float size = 1.0f; // Source-art pixels; rendering chooses physical pixel scale.
    float age = 0.0f;
    float lifetime = 1.0f;
    ParticleKind kind = ParticleKind::flame;
    std::uint64_t emitter_id = 0;
    std::uint64_t birth_ordinal = 0;
};

float fire_intensity(const FireEmitter& emitter, std::uint64_t tick);

class ParticleSystem {
public:
    static constexpr std::size_t max_particles = 4096;
    static constexpr std::size_t max_emitters = 96 * 72;

    explicit ParticleSystem(std::size_t capacity = max_particles);
    void reset(std::uint32_t seed);
    // Refresh after a world update; existing sources retain credits/counters.
    void set_emitters(const std::vector<FireEmitter>& emitters);
    // Consume one shared base tick, with no additional speed multiplier.
    void step(std::uint64_t tick);

    const Particle* data() const { return particles_.data(); }
    std::size_t size() const { return active_count_; }
    std::size_t emitter_count() const { return emitters_.size(); }
    std::uint64_t spawned_count() const { return spawned_count_; }
    std::uint64_t dropped_count() const { return dropped_count_; }

private:
    struct EmitterState {
        FireEmitter view;
        std::array<double, 3> credits{};
        std::array<std::uint64_t, 3> ordinals{};
        float intensity = 0.0f;
    };

    Particle make_particle(const FireEmitter& emitter, ParticleKind kind,
                           std::uint64_t ordinal) const;
    void update_particles();

    std::array<Particle, max_particles> particles_{};
    std::vector<EmitterState> emitters_;
    std::vector<EmitterState> next_emitters_;
    std::vector<FireEmitter> incoming_;
    std::size_t capacity_;
    std::size_t active_count_ = 0;
    std::size_t scheduling_cursor_ = 0;
    std::uint32_t seed_ = 0;
    std::uint64_t last_tick_ = 0;
    std::uint64_t spawned_count_ = 0;
    std::uint64_t dropped_count_ = 0;
};
