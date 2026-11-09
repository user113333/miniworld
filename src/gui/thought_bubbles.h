#pragma once
#include "actors/actor_view.h"
#include "render/actor_scene.h"

// Windowless, non-interactive drawing; visibility is owned by simulation timers.
void draw_thought_bubbles(const std::vector<ActorView>& actors, const ActorSceneView& view);
