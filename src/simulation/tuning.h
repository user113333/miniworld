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

struct SettlerTuning {
    static constexpr float base_speed = 1.0f; // Hex-radius units per second.
    static constexpr int search_radius = 10;
    static constexpr int thought_radius = 5;
    static constexpr int comfortable_people = 2;
    static constexpr int comfortable_trees = 12;
    static constexpr int people_penalty = 35;
    static constexpr int tree_penalty = 3;
    static constexpr int comfort_gain = 10;
    static constexpr int thought_seconds = 10;
    static constexpr int cut_seconds = 3;
    static constexpr int plant_seconds = 2;
    static constexpr int growth_seconds = 60;
};
