#pragma once

#include "render/map_renderer.h"

#include <cmath>

class Camera {
public:
    MapView view(MapView centered) const
    {
        return {centered.origin + offset_, centered.radius * zoom_};
    }

    void pan(glm::vec2 delta) { offset_ += delta; }

    void zoom(float wheel, glm::vec2 cursor, MapView centered)
    {
        const float next = glm::clamp(zoom_ * std::pow(1.15f, wheel), 0.25f, 8.0f);
        // Keep the world point beneath the cursor stationary.
        offset_ = cursor - centered.origin
                  - (cursor - view(centered).origin) * (next / zoom_);
        zoom_ = next;
    }

private:
    glm::vec2 offset_{0.0f};
    float zoom_ = 1.0f;
};
