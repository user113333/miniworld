#include "particle_system.h"
#include "simulation_clock.h"
#include "test_support.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <tuple>

namespace {

using Key = std::tuple<std::uint64_t, ParticleKind, std::uint64_t>;

std::vector<FireEmitter> sources(std::size_t count, std::uint64_t burnout_tick = 640)
{
    std::vector<FireEmitter> result;
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back({index + 1, {static_cast<float>(index) * 0.5f, 0.25f},
                          0.7f + static_cast<float>(index % 3) * 0.35f, 0, burnout_tick});
    }
    return result;
}

std::map<Key, Particle> snapshot(const ParticleSystem& system)
{
    std::map<Key, Particle> result;
    for (std::size_t index = 0; index < system.size(); ++index) {
        const Particle& particle = system.data()[index];
        test::expect(result.emplace(Key{particle.emitter_id, particle.kind, particle.birth_ordinal}, particle).second,
                     "Particle identity is duplicated");
    }
    return result;
}

void expect_same(const ParticleSystem& actual, const ParticleSystem& expected)
{
    const auto a = snapshot(actual);
    const auto b = snapshot(expected);
    test::expect(a.size() == b.size() && actual.spawned_count() == expected.spawned_count()
                 && actual.dropped_count() == expected.dropped_count(), "Replay changed emission counts");
    for (const auto& [key, particle] : a) {
        const auto found = b.find(key);
        test::expect(found != b.end(), "Replay changed particle identity");
        const Particle& other = found->second;
        test::expect(particle.position == other.position && particle.velocity == other.velocity
                     && particle.ground == other.ground && particle.color == other.color
                     && particle.size == other.size && particle.age == other.age
                     && particle.lifetime == other.lifetime, "Replay changed particle state");
    }
}

void check_envelope()
{
    FireEmitter emitter{1, {}, 1.5f, 100, 1100};
    test::expect(fire_intensity(emitter, 0) == 0 && fire_intensity(emitter, 100) == 0,
                 "Fire emits before ignition");
    test::expect(test::near(fire_intensity(emitter, 200), 0.5f), "Fire does not build up gradually");
    test::expect(fire_intensity(emitter, 400) == 1 && fire_intensity(emitter, 850) == 1,
                 "Fire does not sustain its middle intensity");
    test::expect(fire_intensity(emitter, 1000) > 0 && fire_intensity(emitter, 1000) < 0.5f,
                 "Fire does not fade before burnout");
    test::expect(fire_intensity(emitter, 1100) == 0 && fire_intensity(emitter, 1200) == 0,
                 "Burned-out fire continues emitting");
    emitter.burnout_tick = 100;
    test::expect(fire_intensity(emitter, 150) == 0, "Invalid burn interval has intensity");
}

void check_fields_and_tails()
{
    ParticleSystem system;
    system.reset(42);
    const auto emitters = sources(3);
    system.set_emitters(emitters);
    std::set<ParticleKind> kinds;
    bool saw_motion = false;
    for (std::uint64_t tick = 1; tick <= 320; ++tick) {
        system.step(tick);
        for (std::size_t index = 0; index < system.size(); ++index) {
            const Particle& particle = system.data()[index];
            test::expect(std::isfinite(particle.position.x) && std::isfinite(particle.position.y)
                         && std::isfinite(particle.velocity.x) && std::isfinite(particle.velocity.y),
                         "Particle motion is not finite");
            test::expect(particle.lifetime > 0 && particle.age >= 0 && particle.age < particle.lifetime,
                         "Particle age/lifetime is invalid or expired particle survived");
            test::expect(particle.size == 1 || particle.size == 2, "Particle is not an art-pixel square");
            for (int component = 0; component < 4; ++component) {
                test::expect(std::isfinite(particle.color[component]) && particle.color[component] >= 0
                             && particle.color[component] <= 1, "Particle color is invalid");
            }
            test::expect(particle.emitter_id > 0 && particle.emitter_id <= emitters.size(),
                         "Particle lost its emitter ID");
            test::expect(particle.ground == emitters[particle.emitter_id - 1].ground,
                         "Particle lost its depth anchor");
            kinds.insert(particle.kind);
            saw_motion |= particle.age > 0 && particle.position.y < particle.ground.y;
        }
    }
    test::expect(kinds.size() == 3 && saw_motion, "Reference fire omitted a particle kind or motion");
    test::expect(system.size() > 0, "Reference fire produced no live particles");
    const auto spawned = system.spawned_count();
    system.set_emitters({});
    test::expect(system.emitter_count() == 0, "Removed emitter retained scheduling state");
    system.step(321);
    test::expect(system.size() > 0, "Emitter removal destroyed every residual particle");
    for (std::uint64_t tick = 322; tick <= 420; ++tick) {
        system.step(tick);
    }
    test::expect(system.size() == 0 && system.spawned_count() == spawned,
                 "Residual particles did not expire or vanished emitter kept spawning");
}

void check_determinism_and_refresh()
{
    ParticleSystem a(64);
    ParticleSystem b(64);
    a.reset(74129);
    b.reset(74129);
    auto forward = sources(16);
    auto backward = forward;
    std::reverse(backward.begin(), backward.end());
    a.set_emitters(forward);
    b.set_emitters(backward);
    for (std::uint64_t tick = 1; tick <= 640; ++tick) {
        if (tick % 8 == 0) {
            // Refreshing unchanged views must retain fractional emission credits.
            a.set_emitters(forward);
            b.set_emitters(backward);
        }
        a.step(tick);
        b.step(tick);
        if (tick % 32 == 0) {
            expect_same(a, b);
        }
    }
    test::expect(a.dropped_count() > 0, "Replay did not exercise pool saturation");
}

void check_capacity_and_fairness()
{
    ParticleSystem system(8);
    system.reset(7);
    system.set_emitters(sources(16, 1600));
    std::set<std::uint64_t> served;
    std::set<ParticleKind> kinds;
    for (std::uint64_t tick = 1; tick <= 1500; ++tick) {
        system.step(tick);
        test::expect(system.size() <= 8, "Saturation exceeded capacity");
        for (std::size_t index = 0; index < system.size(); ++index) {
            served.insert(system.data()[index].emitter_id);
            kinds.insert(system.data()[index].kind);
        }
    }
    test::expect(served.size() == 16, "Rotating service permanently starved an emitter");
    test::expect(system.dropped_count() > system.spawned_count(), "Overload did not drop cosmetic requests");
    test::expect(kinds.count(ParticleKind::flame) != 0, "Saturated pool omitted flames");

    ParticleSystem disabled(0);
    disabled.reset(7);
    disabled.set_emitters(sources(16));
    for (std::uint64_t tick = 1; tick <= 320; ++tick) {
        disabled.step(tick);
    }
    test::expect(disabled.size() == 0 && disabled.spawned_count() == 0 && disabled.dropped_count() > 0,
                 "Disabled visuals were not bounded");

    ParticleSystem full;
    full.reset(8);
    full.set_emitters(sources(3000, 640));
    std::size_t peak = 0;
    std::set<std::uint64_t> sustained_flames;
    for (std::uint64_t tick = 1; tick <= 640; ++tick) {
        full.step(tick);
        peak = std::max(peak, full.size());
        test::expect(full.size() <= ParticleSystem::max_particles, "Many fires exceeded the shared pool");
        if (tick >= 160 && tick <= 400) {
            for (std::size_t index = 0; index < full.size(); ++index) {
                const Particle& particle = full.data()[index];
                if (particle.kind == ParticleKind::flame) {
                    sustained_flames.insert(particle.emitter_id);
                }
            }
        }
    }
    test::expect(peak == ParticleSystem::max_particles && full.dropped_count() > 0,
                 "Many-fire load did not exercise the full shared pool");
    test::expect(sustained_flames.size() == 3000,
                 "Sustained overload left distant fires without flames");
}

void check_pool_slot_independence()
{
    ParticleSystem roomy(256);
    ParticleSystem tiny(4);
    roomy.reset(29);
    tiny.reset(29);
    const auto emitters = sources(16);
    roomy.set_emitters(emitters);
    tiny.set_emitters(emitters);
    for (std::uint64_t tick = 1; tick <= 500; ++tick) {
        roomy.step(tick);
        tiny.step(tick);
        const auto reference = snapshot(roomy);
        for (const auto& [key, particle] : snapshot(tiny)) {
            const auto found = reference.find(key);
            test::expect(found != reference.end(), "Tiny-pool particle has no unsaturated counterpart");
            const Particle& other = found->second;
            test::expect(particle.position == other.position && particle.velocity == other.velocity
                         && particle.lifetime == other.lifetime && particle.age == other.age
                         && particle.color == other.color && particle.size == other.size,
                         "Cosmetic sampling or integration depends on mutable pool slot");
        }
    }
    test::expect(roomy.dropped_count() == 0 && tiny.dropped_count() > 0,
                 "Pool comparison did not exercise different compaction/allocation histories");
}

void check_tiny_pool_fairness()
{
    ParticleSystem system(1);
    system.reset(19);
    system.set_emitters(sources(16, 1600));
    std::set<std::uint64_t> service_round;
    int flame_grants = 0;
    for (std::uint64_t tick = 1; tick <= 1500; ++tick) {
        system.step(tick);
        if (system.size() == 0 || system.data()[0].kind != ParticleKind::flame
            || system.data()[0].age != 0.0f) {
            continue;
        }
        service_round.insert(system.data()[0].emitter_id);
        if (++flame_grants % 16 == 0) {
            test::expect(service_round.size() == 16,
                         "Tiny-pool service phase-locked onto a subset of equal fires");
            service_round.clear();
        }
    }
    test::expect(flame_grants >= 32, "Tiny-pool fairness did not exercise multiple service rounds");
}

void check_fractional_credit_and_no_backlog()
{
    ParticleSystem system(1);
    system.reset(13);
    system.set_emitters(sources(1, 3200));
    for (std::uint64_t tick = 1; tick <= 1000; ++tick) {
        system.step(tick);
    }
    test::expect(system.dropped_count() > 0, "Tiny pool did not exercise dropped requests");
    // A full pool consumes attempts rather than storing an integer backlog.
    // Each kind can request at most one birth per base tick at these rates.
    std::uint64_t attempts = system.spawned_count() + system.dropped_count();
    for (std::uint64_t tick = 1001; tick <= 1032; ++tick) {
        system.step(tick);
        const auto next = system.spawned_count() + system.dropped_count();
        test::expect(next - attempts <= 3, "Full pool retained a deferred emission burst");
        attempts = next;
    }

    ParticleSystem single;
    single.reset(1);
    single.set_emitters(sources(1, 320));
    for (std::uint64_t tick = 1; tick <= 320; ++tick) {
        single.step(tick);
    }
    // Integral of the envelope is about7.75s:12/s gives about93flames, plus
    // fractional smoke/embers. This broad check catches per-tick rate mistakes.
    test::expect(single.spawned_count() > 90 && single.spawned_count() < 130,
                 "Fractional emission does not follow simulation-second rates");
}

void check_shared_clock_and_reset()
{
    ParticleSystem coarse;
    ParticleSystem fine;
    coarse.reset(91);
    fine.reset(91);
    coarse.set_emitters(sources(4, 2000));
    fine.set_emitters(sources(4, 2000));
    SimulationClock a;
    SimulationClock b;
    const auto frame = [](ParticleSystem& particles, SimulationClock& clock, double elapsed, double speed) {
        clock.accumulate(elapsed, speed);
        int steps = 0;
        while (clock.step()) {
            particles.step(clock.ticks());
            ++steps;
        }
        return steps;
    };
    for (int i = 0; i < 4; ++i) {
        test::expect(frame(coarse, a, 0.25, 50.0) == 400, "50x frame exceeded or lost base steps");
    }
    for (int i = 0; i < 64; ++i) {
        frame(fine, b, 1.0 / 64.0, 50.0);
    }
    expect_same(coarse, fine);
    const auto paused = snapshot(fine);
    const auto spawned = fine.spawned_count();
    const auto dropped = fine.dropped_count();
    for (int i = 0; i < 20; ++i) {
        test::expect(frame(fine, b, 10.0, 0.0) == 0, "Pause advanced effects ticks");
    }
    test::expect(snapshot(fine).size() == paused.size() && fine.spawned_count() == spawned
                 && fine.dropped_count() == dropped, "Pause changed effects scheduling");
    expect_same(coarse, fine);
    fine.reset(91);
    test::expect(fine.size() == 0 && fine.emitter_count() == 0
                 && fine.spawned_count() == 0 && fine.dropped_count() == 0,
                 "Reset retained particles or scheduling state");
    fine.set_emitters(sources(4, 2000));
    ParticleSystem fresh;
    fresh.reset(91);
    fresh.set_emitters(sources(4, 2000));
    for (std::uint64_t tick = 1; tick <= 100; ++tick) {
        fine.step(tick);
        fresh.step(tick);
    }
    expect_same(fine, fresh);
}

void check_invalid_sources()
{
    ParticleSystem system;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    system.set_emitters({{1, {}, 1, 10, 10}, {2, {nan, 0}, 1, 0, 320}, {3, {}, nan, 0, 320}});
    for (std::uint64_t tick = 1; tick <= 100; ++tick) {
        system.step(tick);
    }
    test::expect(system.size() == 0 && system.spawned_count() == 0,
                 "Invalid emitter produced particles");
}

} // namespace

int main()
{
    return test::run("Fire particles", [] {
        check_envelope();
        check_fields_and_tails();
        check_determinism_and_refresh();
        check_capacity_and_fairness();
        check_pool_slot_independence();
        check_tiny_pool_fairness();
        check_fractional_credit_and_no_backlog();
        check_shared_clock_and_reset();
        check_invalid_sources();
    });
}
