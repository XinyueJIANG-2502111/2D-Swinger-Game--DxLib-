#pragma once
#include "Vector.hpp"

class Camera {
public:
    Vector2 position{ 400.0f, 300.0f }; // Viewport center in world coordinates.

    void Init() {
        position = Vector2{ 400.0f, 300.0f };
    }

    // Map world coordinates to screen pixels.
    Vector2 WorldToScreen(const Vector2& worldPos) const {
        return {
            worldPos.x - position.x + 400.0f,
            worldPos.y - position.y + 300.0f
        };
    }

    // Ease the camera toward its follow target.
    void Follow(const Vector2& playerWorldPos, float dt) {
        // Show more of the area above the player.
        float targetX = playerWorldPos.x;
        float targetY = playerWorldPos.y - 80.0f;

        position.x += (targetX - position.x) * 5.0f * dt;
        position.y += (targetY - position.y) * 5.0f * dt;
    }
};