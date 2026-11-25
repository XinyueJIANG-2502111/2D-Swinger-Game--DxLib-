#pragma once

#include "Vector.hpp"
#include <vector>

class Camera;

// A droplet thrown up when the player enters water.
struct SplashParticle {
    Vector2 pos;
    Vector2 vel;
    float alpha;
    float size;
    float lifetime;
};

// A rising particle for the drowning effect.
struct BloodParticle {
    Vector2 pos;
    Vector2 vel;
    float size;
    float alpha;
    float maxLifetime;
    float currentLifetime;
    float wavePhase;
};


class Water {
public:
    struct WaterPoint {
        float y;
        float targetY; // Resting surface height.
        float speed; // Vertical velocity.
    };

    std::vector<WaterPoint> points;
    std::vector<SplashParticle> particles;
    std::vector<BloodParticle> bloodParticles;

    // Spring and wave parameters.
    const float k = 2.5f; // Spring stiffness.
    const float damping = 0.05f; // Spring damping.
    const float spread = 0.25f; // Coupling between neighbors.

    const float surfaceY = 550.0f; // Resting surface in world coordinates.
    const int numPoints = 81;
    const float spacing = 10.0f; // Horizontal distance between springs.

public:
    Water();

    // Flatten the surface and clear all water particles.
    void Reset();

    // Disturb the surface and spawn droplets at the impact point.
    void Splash(float playerWorldX, float velocityY, float cameraLeft);

    // Spawn particles for the drowning effect.
    void EmitBlood(Vector2 worldPos, int count);

    void Draw(const Camera& camera);

    void Update(float dt);
};
