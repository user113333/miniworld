#pragma once
#include "actors/actor.h"

struct IgniteActor { ActorHandle target; };

struct SpawnSettler { ActorPosition position; std::uint64_t generation; };
