#pragma once

#include <cstdint>

class SimulationClock {
public:
    static constexpr double base_step_seconds = 1.0 / 32.0;
    static constexpr std::uint64_t ticks_per_world_step = 8;
    static constexpr double max_speed = 50.0;
    static constexpr double max_frame_seconds = 0.25;

    // Admit one frame, then drain step() before admitting another frame.
    // Inactive/paused frames preserve the fractional remainder, not wall time.
    void accumulate(double real_seconds, double speed, bool active = true);
    bool step();
    void reset();

    std::uint64_t ticks() const { return ticks_; }
    double elapsed_seconds() const { return ticks_ * base_step_seconds; }
    bool world_update_due() const
    {
        return ticks_ != 0 && ticks_ % ticks_per_world_step == 0;
    }

private:
    long double accumulated_ticks_ = 0.0L;
    std::uint64_t ticks_ = 0;
};
