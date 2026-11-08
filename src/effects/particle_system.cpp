#include "effects/particle_system.h"
#include "simulation/simulation_clock.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

constexpr float base_seconds = static_cast<float>(SimulationClock::base_step_seconds);
constexpr std::array<double, 3> emission_rates{12.0, 0.75, 1.5};

float smoothstep(float minimum, float maximum, float value)
{
    const float u = glm::clamp((value - minimum) / (maximum - minimum), 0.0f, 1.0f);
    return u * u * (3.0f - 2.0f * u);
}

std::uint64_t mix(std::uint64_t value)
{
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

float sample(std::uint32_t seed, const FireEmitter& emitter, ParticleKind kind,
             std::uint64_t ordinal, std::uint64_t channel)
{
    // A separate cosmetic stream: no world random state or pool-slot identity.
    const std::uint64_t hash = mix(mix(seed ^ 0x4fb723e4d951bc17ULL) ^ mix(emitter.id)
        ^ mix(ordinal + 0x9e3779b97f4a7c15ULL)
        ^ mix(static_cast<std::uint64_t>(kind) + 0x6a09e667f3bcc909ULL)
        ^ mix(channel + 0xbb67ae8584caa73bULL));
    return static_cast<float>(hash >> 40) / 16777216.0f;
}

bool same_emitter(const FireEmitter& a, const FireEmitter& b)
{
    return a.id == b.id && a.ground == b.ground && a.height == b.height
           && a.ignition_tick == b.ignition_tick && a.burnout_tick == b.burnout_tick;
}

bool valid_geometry(const FireEmitter& emitter)
{
    return emitter.id != 0 && std::isfinite(emitter.ground.x) && std::isfinite(emitter.ground.y)
           && std::isfinite(emitter.height) && emitter.height > 0.0f;
}

void update_appearance(Particle& particle)
{
    const float u = particle.age / particle.lifetime;
    switch (particle.kind) {
    case ParticleKind::flame:
        particle.color = u < 0.3f ? glm::vec4{1.0f, 0.86f, 0.2f, 1.0f}
                         : u < 0.7f ? glm::vec4{1.0f, 0.42f, 0.05f, 1.0f}
                                    : glm::vec4{0.8f, 0.12f, 0.02f, 0.85f};
        if (u > 0.65f) {
            particle.size = 1.0f;
        }
        break;
    case ParticleKind::smoke:
        particle.color = {0.38f, 0.37f, 0.35f, 0.45f * (1.0f - u)};
        break;
    case ParticleKind::ember:
        particle.color = u < 0.35f ? glm::vec4{1.0f, 0.9f, 0.3f, 1.0f}
                         : u < 0.7f ? glm::vec4{1.0f, 0.4f, 0.06f, 1.0f}
                                    : glm::vec4{0.65f, 0.1f, 0.02f, 1.0f - u};
        break;
    }
}

} // namespace

float fire_intensity(const FireEmitter& emitter, std::uint64_t tick)
{
    if (emitter.burnout_tick <= emitter.ignition_tick || tick <= emitter.ignition_tick
        || tick >= emitter.burnout_tick) {
        return 0.0f;
    }
    const float progress = static_cast<float>(tick - emitter.ignition_tick)
                           / static_cast<float>(emitter.burnout_tick - emitter.ignition_tick);
    return smoothstep(0.0f, 0.2f, progress) * (1.0f - smoothstep(0.75f, 1.0f, progress));
}

ParticleSystem::ParticleSystem(std::size_t capacity) : capacity_(capacity)
{
    if (capacity > max_particles) {
        throw std::invalid_argument("Particle capacity exceeds the fixed pool");
    }
    emitters_.reserve(max_emitters);
    next_emitters_.reserve(max_emitters);
    incoming_.reserve(max_emitters);
}

void ParticleSystem::reset(std::uint32_t seed)
{
    seed_ = seed;
    active_count_ = 0;
    scheduling_cursor_ = 0;
    last_tick_ = 0;
    spawned_count_ = 0;
    dropped_count_ = 0;
    emitters_.clear();
    next_emitters_.clear();
    incoming_.clear();
}

void ParticleSystem::set_emitters(const std::vector<FireEmitter>& emitters)
{
    if (emitters.size() > max_emitters) {
        throw std::invalid_argument("Too many fire emitters for the bounded world");
    }
    if (incoming_.size() == emitters.size()
        && std::equal(incoming_.begin(), incoming_.end(), emitters.begin(), same_emitter)) {
        return;
    }
    next_emitters_.clear();
    for (const FireEmitter& emitter : emitters) {
        next_emitters_.push_back({emitter});
    }
    std::sort(next_emitters_.begin(), next_emitters_.end(), [](const EmitterState& a, const EmitterState& b) {
        return a.view.id < b.view.id;
    });
    std::size_t previous = 0;
    for (std::size_t index = 0; index < next_emitters_.size(); ++index) {
        EmitterState& state = next_emitters_[index];
        if (index > 0 && state.view.id == next_emitters_[index - 1].view.id) {
            throw std::invalid_argument("Duplicate fire emitter ID");
        }
        while (previous < emitters_.size() && emitters_[previous].view.id < state.view.id) {
            ++previous;
        }
        if (previous < emitters_.size() && emitters_[previous].view.id == state.view.id) {
            state.credits = emitters_[previous].credits;
            state.ordinals = emitters_[previous].ordinals;
        }
    }
    incoming_ = emitters;
    emitters_.swap(next_emitters_);
    scheduling_cursor_ = emitters_.empty() ? 0 : scheduling_cursor_ % emitters_.size();
}

Particle ParticleSystem::make_particle(const FireEmitter& emitter, ParticleKind kind,
                                      std::uint64_t ordinal) const
{
    const auto random = [&](std::uint64_t channel) { return sample(seed_, emitter, kind, ordinal, channel); };
    Particle particle;
    particle.kind = kind;
    particle.emitter_id = emitter.id;
    particle.birth_ordinal = ordinal;
    particle.ground = emitter.ground;
    particle.position = emitter.ground + glm::vec2{(random(0) - 0.5f) * emitter.height * 0.5f,
                                                  -(0.2f + random(1) * 0.5f) * emitter.height};
    switch (kind) {
    case ParticleKind::flame:
        particle.velocity = {(random(2) - 0.5f) * 0.5f, -1.8f - random(3) * 1.2f};
        particle.lifetime = 0.35f + random(4) * 0.35f;
        particle.size = random(5) < 0.5f ? 1.0f : 2.0f;
        break;
    case ParticleKind::smoke:
        particle.velocity = {(random(2) - 0.5f) * 0.4f, -0.5f - random(3) * 0.4f};
        particle.lifetime = 1.0f + random(4) * 1.5f;
        particle.size = random(5) < 0.5f ? 1.0f : 2.0f;
        break;
    case ParticleKind::ember:
        particle.velocity = {(random(2) - 0.5f) * 1.6f, -1.8f - random(3) * 1.2f};
        particle.lifetime = 0.4f + random(4) * 0.8f;
        break;
    }
    update_appearance(particle);
    return particle;
}

void ParticleSystem::update_particles()
{
    for (std::size_t index = 0; index < active_count_;) {
        Particle& particle = particles_[index];
        particle.age += base_seconds;
        if (particle.age >= particle.lifetime) {
            particle = particles_[--active_count_];
            continue; // The swapped-in entry still needs this tick's update.
        }
        if (particle.kind == ParticleKind::ember) {
            particle.velocity.y += 1.6f * base_seconds;
        }
        particle.position += particle.velocity * base_seconds;
        update_appearance(particle);
        ++index;
    }
}

void ParticleSystem::step(std::uint64_t tick)
{
    if (tick != last_tick_ + 1) {
        throw std::invalid_argument("Particles require successive base ticks");
    }
    update_particles();
    for (EmitterState& state : emitters_) {
        state.intensity = valid_geometry(state.view) ? fire_intensity(state.view, tick) : 0.0f;
    }
    std::size_t next_cursor = scheduling_cursor_;
    for (std::size_t kind = 0; kind < emission_rates.size(); ++kind) {
        for (std::size_t offset = 0; offset < emitters_.size(); ++offset) {
            const std::size_t emitter_index = (scheduling_cursor_ + offset) % emitters_.size();
            EmitterState& state = emitters_[emitter_index];
            const double intensity = state.intensity;
            double& credit = state.credits[kind];
            credit += emission_rates[kind] * intensity * base_seconds;
            const auto attempts = static_cast<std::uint64_t>(credit);
            credit -= static_cast<double>(attempts);
            for (std::uint64_t attempt = 0; attempt < attempts; ++attempt) {
                const std::uint64_t ordinal = state.ordinals[kind]++;
                if (active_count_ == capacity_) {
                    ++dropped_count_;
                    continue;
                }
                particles_[active_count_++] = make_particle(state.view, static_cast<ParticleKind>(kind), ordinal);
                ++spawned_count_;
                if (kind == static_cast<std::size_t>(ParticleKind::flame)) {
                    // Advance past the whole serviced span. A one-source stride
                    // can leave distant fires cold throughout a short mass burn.
                    next_cursor = (emitter_index + 1) % emitters_.size();
                }
            }
        }
    }
    scheduling_cursor_ = next_cursor;
    last_tick_ = tick;
}
