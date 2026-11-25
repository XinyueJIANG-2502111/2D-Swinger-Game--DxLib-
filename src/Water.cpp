#include "Water.hpp"
#include "Vector.hpp"
#include "Camera.hpp"

#include "DxLib.h"
#include <random>

// The original comments in this file were generated with Gemini.

Water::Water() {
    Reset();
}

void Water::Reset() {
    // Restore all springs to a flat, still surface.
    points.resize(numPoints);
    for (int i = 0; i < numPoints; i++) {
        points[i].y = surfaceY;
        points[i].targetY = surfaceY;
        points[i].speed = 0.0f;
    }

    // Clear particles from the previous run.
    particles.clear();
    bloodParticles.clear();
}

void Water::Splash(float playerWorldX, float velocityY, float cameraLeft) {
    // Map the world X position to a camera-relative spring index.
    float relativeX = playerWorldX - cameraLeft;
    int index = static_cast<int>(relativeX / spacing);

    // Kick the spring downward and cap its speed.
    if (index >= 0 && index < numPoints) {
        float force = velocityY * 0.4f;
        points[index].speed = force > 300.0f ? 300.0f : force;
    }

    // Create more splash particles for faster impacts.
    int particleCount = static_cast<int>(velocityY * 0.05f);
    for (int i = 0; i < particleCount; i++) {
        SplashParticle p;
        p.pos = { playerWorldX, surfaceY };

        // Spray particles in an upward fan.
        float angle = ((rand() % 100) / 100.0f) * 3.14159f * 0.6f + 3.14159f * 0.2f;
        float speed = ((rand() % 100) / 100.0f) * (velocityY * 0.6f) + 100.0f;
        p.vel = { cosf(angle) * speed, -sinf(angle) * speed };

        p.alpha = 1.0f; p.size = 3.0f + (rand() % 4); p.lifetime = 0.5f + ((rand() % 50) / 100.0f);
        particles.push_back(p);
    }
}

void Water::EmitBlood(Vector2 worldPos, int count) {
    int actualCount = count * 6; // Scale the particle density.

    for (int i = 0; i < actualCount; i++) {
        BloodParticle p;

        // Scatter particles around the emission point.
        float offset = ((rand() % 100) / 100.0f) * 8.0f;
        float angle = ((rand() % 100) / 100.0f) * 2.0f * 3.14159f;
        p.pos.x = worldPos.x + cosf(angle) * offset;
        p.pos.y = worldPos.y + sinf(angle) * offset;

        // Give particles an upward speed and a little sideways drift.
        p.vel.x = ((rand() % 20) - 10.0f) * 0.5f;
        p.vel.y = -(((rand() % 40) + 40.0f));

        p.size = 1.0f + ((rand() % 120) / 100.0f);
        p.alpha = 0.15f + ((rand() % 15) / 100.0f);
        p.maxLifetime = 2.0f + ((rand() % 100) / 100.0f) * 1.5f;
        p.currentLifetime = p.maxLifetime;

        // Start each particle at a random phase of its sideways sway.
        p.wavePhase = ((rand() % 100) / 100.0f) * 6.28f;

        bloodParticles.push_back(p);
    }
}

void Water::Draw(const Camera& camera) {
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 130);
    int waterColor = GetColor(134, 214, 217);
    int surfaceColor = GetColor(0, 230, 194);

    float worldLeft = camera.position.x - 400.0f;

    // Join adjacent springs into quads across the viewport.
    for (int i = 0; i < numPoints - 1; i++) {
        float wx1 = worldLeft + static_cast<float>(i * spacing);
        float wx2 = worldLeft + static_cast<float>((i + 1) * spacing);

        float wy1 = points[i].y;
        float wy2 = points[i + 1].y;

        Vector2 sW1 = camera.WorldToScreen({ wx1, wy1 });
        Vector2 sW2 = camera.WorldToScreen({ wx2, wy2 });

        // Extend the water fill below the playable area.
        Vector2 sB1 = camera.WorldToScreen({ wx1, 2500.0f });
        Vector2 sB2 = camera.WorldToScreen({ wx2, 2500.0f });

        DrawQuadrangle(
            static_cast<int>(sW1.x), static_cast<int>(sW1.y),
            static_cast<int>(sW2.x), static_cast<int>(sW2.y),
            static_cast<int>(sB2.x), static_cast<int>(sB2.y),
            static_cast<int>(sB1.x), static_cast<int>(sB1.y),
            waterColor, TRUE
        );

        DrawLine(static_cast<int>(sW1.x), static_cast<int>(sW1.y), static_cast<int>(sW2.x), static_cast<int>(sW2.y), surfaceColor, 2);
    }

    // Draw the underwater death effect.
    for (const auto& bp : bloodParticles) {
        Vector2 sPos = camera.WorldToScreen(bp.pos);
        // Skip particles off screen.
        if (sPos.x < -10 || sPos.x > 810 || sPos.y < -10 || sPos.y > 610) continue;

        int alphaVal = static_cast<int>(bp.alpha * 255);
        if (alphaVal <= 0) continue;

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alphaVal);
        DrawCircle(static_cast<int>(sPos.x), static_cast<int>(sPos.y),
            static_cast<int>(bp.size), GetColor(200, 0, 20), TRUE);
    }

    // Draw splash droplets.
    for (const auto& p : particles) {
        Vector2 sParticlePos = camera.WorldToScreen(p.pos);
        int a = static_cast<int>(p.alpha * 200);
        if (a < 0) a = 0;
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, a);
        DrawCircle(static_cast<int>(sParticlePos.x), static_cast<int>(sParticlePos.y), static_cast<int>(p.size), GetColor(100, 220, 255), TRUE);
    }

    // Restore the blend mode for later drawing.
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void Water::Update(float dt) {
    // Update each surface point as a damped spring.
    for (int i = 0; i < numPoints; i++) {
        float diff = points[i].y - points[i].targetY;
        float acceleration = -k * diff - damping * points[i].speed;
        points[i].speed += acceleration * dt * 60.0f; // Scale acceleration to a 60 Hz reference rate.
        points[i].y += points[i].speed * dt;
    }

    // Spread each spring displacement to its neighbors.
    std::vector<float> leftDeltas(numPoints, 0.0f);
    std::vector<float> rightDeltas(numPoints, 0.0f);

    // Repeat the spread eight times per frame.
    for (int j = 0; j < 8; j++) {
        for (int i = 0; i < numPoints; i++) {
            if (i > 0) {
                leftDeltas[i] = spread * (points[i].y - points[i - 1].y);
                points[i - 1].speed += leftDeltas[i] * dt * 60.0f;
            }
            if (i < numPoints - 1) {
                rightDeltas[i] = spread * (points[i].y - points[i + 1].y);
                points[i + 1].speed += rightDeltas[i] * dt * 60.0f;
            }
        }

        // Apply displacement after computing all neighbor deltas for this pass.
        for (int i = 0; i < numPoints; i++) {
            if (i > 0) points[i - 1].y += leftDeltas[i] * dt;
            if (i < numPoints - 1) points[i + 1].y += rightDeltas[i] * dt;
        }
    }


    // Apply gravity to droplets and fade them over their lifetime.
    for (size_t i = 0; i < particles.size(); ) {
        particles[i].vel.y += 800.0f * dt;
        particles[i].pos += particles[i].vel * dt;
        particles[i].lifetime -= dt;
        particles[i].alpha = particles[i].lifetime / 1.0f;

        if (particles[i].lifetime <= 0.0f || particles[i].alpha <= 0.0f) {
            particles.erase(particles.begin() + i);
        }
        else {
            i++;
        }
    }


    const float WATER_LINE_Y = 550.0f; // Resting waterline.

    // Update the underwater particles.
    for (auto it = bloodParticles.begin(); it != bloodParticles.end(); ) {
        it->currentLifetime -= dt;

        // Remove expired particles and particles that reach above the waterline.
        if (it->currentLifetime <= 0.0f || it->pos.y < WATER_LINE_Y) {
            it = bloodParticles.erase(it);
        }
        else {
            // Accelerate upward to a maximum rise speed.
            it->vel.y -= 30.0f * dt;
            if (it->vel.y < -90.0f) it->vel.y = -90.0f;

            // Add a sinusoidal sideways drift.
            it->wavePhase += dt * 5.0f;
            float swing = sinf(it->wavePhase) * 1.2f;

            it->pos.y += it->vel.y * dt;
            it->pos.x += (it->vel.x + swing) * dt;

            // Fade particles as they approach within 60 pixels of the surface.
            float distanceToSurface = it->pos.y - WATER_LINE_Y;
            float surfaceAlphaScale = 1.0f;
            if (distanceToSurface < 60.0f) {
                surfaceAlphaScale = distanceToSurface / 60.0f;
            }

            // Combine the lifetime and surface fades.
            float lifeRatio = it->currentLifetime / it->maxLifetime;
            it->alpha = (lifeRatio * 0.25f) * surfaceAlphaScale;

            // Grow particles as they disperse.
            it->size += dt * 0.5f;

            it++;
        }
    }
}
