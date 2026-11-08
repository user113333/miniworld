#pragma once

#include "simulation/simulation_clock.h"
#include "world/tile_map.h"
#include <cstdint>

namespace simulation_detail {

constexpr std::uint64_t world_ticks = SimulationClock::ticks_per_world_step;
constexpr double world_seconds = world_ticks * SimulationClock::base_step_seconds;

enum class Sample : std::uint64_t {
    birth,
    variant,
    offset_x,
    offset_y,
    growth,
    lifespan,
    decay,
    initial_age,
    spread,
    burn_duration,
    charred_decay,
    recovery_duration
};

inline std::uint64_t mix(std::uint64_t value)
{
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

inline std::uint64_t sample(std::uint32_t seed, std::uint64_t tick,
                     std::uint64_t identity, Sample channel)
{
    // This keyed stream is separate from initial terrain/forest generation.
    return mix(mix(seed ^ 0x54ce64d9e8c1c3b7ULL) ^ mix(tick)
               ^ mix(identity + 0x9e3779b97f4a7c15ULL)
               ^ mix(static_cast<std::uint64_t>(channel) + 0xd1b54a32d192ed03ULL));
}

inline double unit_sample(std::uint32_t seed, std::uint64_t tick,
                   std::uint64_t identity, Sample channel)
{
    return static_cast<double>(sample(seed, tick, identity, channel) >> 11)
           / 9007199254740992.0;
}

inline std::uint64_t duration(std::uint32_t seed, std::uint64_t identity, Sample channel,
                       double minimum_seconds, double maximum_seconds)
{
    const auto minimum = static_cast<std::uint64_t>(minimum_seconds / world_seconds);
    const auto maximum = static_cast<std::uint64_t>(maximum_seconds / world_seconds);
    return (minimum + sample(seed, 0, identity, channel) % (maximum - minimum + 1)) * world_ticks;
}

inline bool can_grow(Terrain terrain)
{
    return terrain == Terrain::grass || terrain == Terrain::land;
}

} // namespace simulation_detail
