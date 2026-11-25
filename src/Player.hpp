#pragma once

#include "Vector.hpp"

#include <vector>

class TerrainManager;
class Water;
class Camera;

// A fading mark left by the moving player.
struct TrailPoint {
    Vector2 pos;
    float alpha;
};

class Player {
public:
    Vector2 position{ 400.0f, 500.0f };
    Vector2 velocity{ 0.0f, 0.0f };

    // Movement and grapple tuning.
    const float gravity = 750.0f;
    const float drag = 0.3f;
    const float swingForce = 600.0f;
    const float reelSpeed = 300.0f;

    const float groundAcc = 1200.0f;
    const float airControlForce = 400.0f;

    bool isGrounded = false;

    // Use this radius for collisions, water entry, and submerged depth.
    const float collisionRadius = 16.0f;
    const float jumpForce = 500.0f;

    const float grappleJumpBonus = 350.0f;
    const float launchPullForce = 350.0f;
    const float ropeLengthRate = 0.9f;
    const float autoPullSpeed = 500.0f;
    float targetRopeLength = 0.0f;

    // Water physics.
    const float waterLineY = 550.0f;
    const float waterDensity = 4.5f; // Water drag coefficient.
    const float buoyancyEngine = 1800.0f;

    // Times in seconds spent in water without input.
    const float buoyancyFadeStartTime = 5.0f;
    const float drowningTime = 10.0f;

    // True while the hook is pinned.
    bool isGrappling = false;
    Vector2 anchorPoint{ 0.0f, 0.0f };
    float ropeLength = 0.0f;

    // Movement trail and water entry state.
    std::vector<TrailPoint> trail;
    bool wasInWater = false;

    // Drowning state.
    float noInputWaterTimer = 0.0f;
    bool isDead = false;
    float buoyancyScale = 1.0f;

    enum class GrappleState {
        None, // Hook is inactive.
        Flying, // Hook is in flight.
        Pinned // Hook is attached to terrain.
    };

    GrappleState grappleState = GrappleState::None;

    Vector2 hookCurrentPos{ 0.0f, 0.0f };
    Vector2 hookVelocity{ 0.0f, 0.0f };
    const float hookFlySpeed = 2200.0f;
    float currentFlyDist = 0.0f; // Distance traveled by the flying hook.
    const float MAX_GRAPPLE_RANGE = 600.0f;


public:
    // Reset movement, grapple, effects, and drowning state for a new spawn.
    void Reset(const Vector2& spawnPosition);

    // Check if the bottom of the player reaches the waterline.
    bool IsInWater() const;

    void LaunchGrapple(Vector2 mouseWorldPos, const TerrainManager& terrain);

    void ReleaseGrapple();

    void Jump();

    void Update(float dt, 
        int horizontalInput, int verticalInput, bool jumpInput, bool reelInput, 
        Water& water, float cameraLeft, const TerrainManager& terrain);

    void Draw(const Camera& camera, const Vector2& mouseWorldPos, const TerrainManager& terrain);

private:
    // Move in substeps and resolve collisions after each step.
    void MoveAndCollide(float dt, const TerrainManager& terrain);

    // Probe below the player to detect resting contact.
    void RefreshGroundedState(const TerrainManager& terrain);
};
