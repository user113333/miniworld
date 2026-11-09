#pragma once

// Durations are simulation seconds.
struct FireTuning {
    static constexpr double burn_seconds_min = 12.0;
    static constexpr double burn_seconds_max = 20.0;
    static constexpr double charred_decay_seconds_min = 15.0;
    static constexpr double charred_decay_seconds_max = 30.0;
    static constexpr double spread_rate = 0.15;
    static constexpr double recovery_seconds_min = 90.0;
    static constexpr double recovery_seconds_max = 180.0;
};
