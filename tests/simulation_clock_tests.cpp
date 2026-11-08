#include "simulation_clock.h"
#include "test_support.h"

#include <limits>

namespace {

struct StepCounts {
    int base = 0;
    int world = 0;
};

StepCounts advance(SimulationClock& clock, double real_seconds, double speed = 1.0,
                   bool active = true)
{
    clock.accumulate(real_seconds, speed, active);
    StepCounts counts;
    while (clock.step()) {
        ++counts.base;
        counts.world += clock.world_update_due();
    }
    return counts;
}

void check_boundaries_and_cadence()
{
    SimulationClock clock;
    test::expect(clock.ticks() == 0 && clock.elapsed_seconds() == 0.0,
                 "New clock is not at time zero");
    test::expect(!clock.world_update_due() && !clock.step(), "New clock has a pending step");
    constexpr double remainder = 1e-9;
    test::expect(advance(clock, SimulationClock::base_step_seconds - remainder).base == 0,
                 "A partial tick advanced time");
    test::expect(advance(clock, remainder).base == 1, "Exact boundary lost a tick");
    test::expect(clock.ticks() == 1 && clock.elapsed_seconds() == SimulationClock::base_step_seconds,
                 "Completed tick has the wrong elapsed time");
    for (int tick = 2; tick <= 32; ++tick) {
        const StepCounts counts = advance(clock, SimulationClock::base_step_seconds);
        test::expect(counts.base == 1, "Base cadence is not 32 Hz");
        test::expect(counts.world == (tick % 8 == 0), "World cadence is not every eight ticks");
    }
    test::expect(clock.elapsed_seconds() == 1.0, "32 base ticks are not one second");
}

void check_speed_and_pause()
{
    for (double speed : {0.0, 0.5, 1.0, 2.0, 4.0, 8.0, 25.0, 50.0}) {
        SimulationClock clock;
        const StepCounts counts = advance(clock, 0.25, speed);
        test::expect(counts.base == static_cast<int>(8 * speed), "Speed ratio changed");
        test::expect(clock.elapsed_seconds() == 0.25 * speed, "Speed has incorrect elapsed time");
    }
    SimulationClock clock;
    advance(clock, SimulationClock::base_step_seconds / 2);
    for (int frame = 0; frame < 100; ++frame) {
        test::expect(advance(clock, 10.0, 0.0).base == 0, "Paused frame advanced the clock");
    }
    test::expect(advance(clock, SimulationClock::base_step_seconds / 2).base == 1,
                 "Pause discarded fractional time or retained paused wall time");
    test::expect(!clock.step(), "Pause accumulated a catch-up backlog");
}

void check_frame_partitions()
{
    for (double speed : {0.5, 1.0, 3.0, 8.0, 25.0, 50.0}) {
        for (int frames : {4, 10, 60, 144, 1000}) {
            SimulationClock clock;
            int world_steps = 0;
            for (int frame = 0; frame < frames; ++frame) {
                world_steps += advance(clock, 1.0 / frames, speed).world;
            }
            test::expect(clock.ticks() == static_cast<std::uint64_t>(32 * speed),
                         "Equal-total frame partitions produced different tick counts");
            test::expect(world_steps == static_cast<int>(4 * speed),
                         "Equal-total frame partitions produced different world counts");
        }
    }
    SimulationClock uneven;
    for (double delta : {0.013, 0.137, 0.25, 0.1, 0.001, 0.249, 0.25}) {
        advance(uneven, delta);
    }
    test::expect(uneven.ticks() == 32, "Uneven decimal partitions lost a fixed step");
}

void check_stalls_and_inactivity()
{
    SimulationClock clock;
    // Retaining an ordinary fractional tick must not exceed the frame work bound.
    advance(clock, SimulationClock::base_step_seconds / 2);
    const StepCounts stall = advance(clock, 60.0, 50.0);
    test::expect(stall.base == 400 && stall.world == 50, "Stall exceeded the fixed work bound");
    test::expect(advance(clock, 0.0).base == 0, "Discarded stall time became a backlog");
    test::expect(advance(clock, SimulationClock::base_step_seconds / 2).base == 1,
                 "Stall handling lost the ordinary fractional remainder");

    SimulationClock inactive;
    advance(inactive, SimulationClock::base_step_seconds / 2);
    for (int frame = 0; frame < 10; ++frame) {
        test::expect(advance(inactive, 60.0, 50.0, false).base == 0,
                     "Inactive frame advanced simulation time");
    }
    test::expect(advance(inactive, SimulationClock::base_step_seconds / 2).base == 1,
                 "Resume retained inactive wall time or discarded the fractional tick");
    test::expect(!inactive.step(), "Inactive time produced offline catch-up");
}

void check_invalid_input_and_reset()
{
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    SimulationClock clock;
    advance(clock, SimulationClock::base_step_seconds / 2);
    for (double delta : {-1.0, -infinity, infinity, nan}) {
        test::expect(advance(clock, delta).base == 0, "Invalid elapsed time advanced the clock");
    }
    for (double speed : {-1.0, -infinity, infinity, nan}) {
        test::expect(advance(clock, 0.25, speed).base == 0, "Invalid speed advanced the clock");
    }
    test::expect(advance(clock, SimulationClock::base_step_seconds / 2).base == 1,
                 "Invalid inputs damaged the fractional accumulator");
    test::expect(advance(clock, 0.25, 100.0).base == 400, "Excess speed was not clamped to 50x");

    clock.accumulate(0.25, 50.0);
    clock.reset();
    test::expect(clock.ticks() == 0 && clock.elapsed_seconds() == 0.0 && !clock.world_update_due(),
                 "Reset retained completed simulation time");
    test::expect(!clock.step(), "Reset retained pending fixed steps");
    advance(clock, SimulationClock::base_step_seconds / 2);
    clock.reset();
    test::expect(advance(clock, SimulationClock::base_step_seconds / 2).base == 0,
                 "Reset retained a fractional tick");
    test::expect(advance(clock, SimulationClock::base_step_seconds / 2).base == 1,
                 "Clock did not resume cleanly after reset");
}

} // namespace

int main()
{
    return test::run("Simulation clock", [] {
        check_boundaries_and_cadence();
        check_speed_and_pause();
        check_frame_partitions();
        check_stalls_and_inactivity();
        check_invalid_input_and_reset();
    });
}
