#pragma once
#include "actors/actor.h"

class Simulation;

class ActorInspector {
public:
    void select(ActorHandle actor);
    void draw(const Simulation& simulation);

private:
    ActorHandle actor_;
    bool focus_requested_ = false;
};
