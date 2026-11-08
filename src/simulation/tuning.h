#pragma once

// Durations are simulation seconds; birth rates are per empty cell per second.
struct ForestTuning {
    static constexpr double growth_seconds_min = 15.0;
    static constexpr double growth_seconds_max = 30.0;
    static constexpr double lifespan_seconds_min = 180.0;
    static constexpr double lifespan_seconds_max = 360.0;
    static constexpr double decay_seconds_min = 30.0;
    static constexpr double decay_seconds_max = 60.0;
    static constexpr double grass_birth_rate = 0.002;
    static constexpr double land_birth_rate = 0.0005;
    static constexpr double habitat_birth_scale = 16.0;
};

struct FireTuning {
    static constexpr double burn_seconds_min = 12.0;
    static constexpr double burn_seconds_max = 20.0;
    static constexpr double dead_burn_seconds_min = 6.0;
    static constexpr double dead_burn_seconds_max = 10.0;
    static constexpr double charred_decay_seconds_min = 15.0;
    static constexpr double charred_decay_seconds_max = 30.0;
    static constexpr double spread_rate = 0.15;
    static constexpr double recovery_seconds_min = 90.0;
    static constexpr double recovery_seconds_max = 180.0;
};
