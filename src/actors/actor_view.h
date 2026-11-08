#pragma once

#include "actor.h"
#include "tree.h"
#include <optional>

// Short-lived value data for rendering, effects and inspection. Never owns actors.
struct ActorView {
    ActorHandle handle;
    ActorId id;
    ActorPosition position;
    ActorAppearance appearance;
    bool selectable = false;
    std::optional<Tree> tree;
};
