#include "simulation_clock.h"

#include <algorithm>
#include <cmath>
#include <limits>

void SimulationClock::accumulate(double real_seconds, double speed, bool active)
{
    if (!active || !std::isfinite(real_seconds) || real_seconds <= 0.0 ||
        !std::isfinite(speed)) {
        return;
    }
    const long double admitted_seconds = std::min(real_seconds, max_frame_seconds);
    const long double admitted_speed = std::clamp(speed, 0.0, max_speed);
    accumulated_ticks_ += admitted_seconds * admitted_speed / base_step_seconds;
}

bool SimulationClock::step()
{
    // Decimal frame partitions can sum just below an exact tick boundary.
    // Keep extra precision and forgive only double-input rounding at that boundary.
    constexpr long double boundary_tolerance = 16.0L * std::numeric_limits<double>::epsilon();
    if (accumulated_ticks_ + boundary_tolerance < 1.0L) {
        return false;
    }
    accumulated_ticks_ = std::max(0.0L, accumulated_ticks_ - 1.0L);
    ++ticks_;
    return true;
}

void SimulationClock::reset()
{
    accumulated_ticks_ = 0.0L;
    ticks_ = 0;
}
