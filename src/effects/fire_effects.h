#pragma once

#include "effects/particle_system.h"
#include "actors/actor_view.h"

// Adapter from authoritative trees to read-only visual sources.
void collect_fire_emitters(const std::vector<ActorView>& actors, std::vector<FireEmitter>& emitters);
