#include "Player.hpp"
#include "DxLib.h"
#include "Vector.hpp"
#include "Water.hpp"
#include "Camera.hpp"
#include "Terrain.hpp"

#include <cmath>
#include <vector>

void Player::Reset(const Vector2& spawnPosition) {
    position = spawnPosition;
    velocity = { 0.0f, 0.0f };
    isGrounded = false;

    ReleaseGrapple();
    anchorPoint = { 0.0f, 0.0f };
    ropeLength = 0.0f;
    targetRopeLength = 0.0f;
    hookCurrentPos = { 0.0f, 0.0f };
    hookVelocity = { 0.0f, 0.0f };
    currentFlyDist = 0.0f;

    trail.clear();
    wasInWater = false;
    noInputWaterTimer = 0.0f;
    isDead = false;
    buoyancyScale = 1.0f;
}

bool Player::IsInWater() const {
    // Water entry starts when the bottom of the player touches the surface.
    return position.y + collisionRadius >= waterLineY;
}

void Player::LaunchGrapple(Vector2 mouseWorldPos, const TerrainManager& terrain) {
    if (grappleState != GrappleState::None) return; // Only one hook can be active.

    Vector2 toMouse = mouseWorldPos - position;
    float distToMouse = toMouse.Length();
    if (distToMouse < 10.0f) return;

    Vector2 rayDir = toMouse.Normalized();

    grappleState = GrappleState::Flying;
    isGrappling = false;

    hookCurrentPos = position;
    hookVelocity = rayDir * hookFlySpeed;
    currentFlyDist = 0.0f;
    anchorPoint = hookCurrentPos;

    // Leave a trail mark when the hook fires.
    trail.push_back({ position, 1.5f });
}

void Player::ReleaseGrapple() {
    isGrappling = false;
    grappleState = GrappleState::None;
}

void Player::Jump() {
    if (isGrappling || isGrounded) {
        ReleaseGrapple();
        velocity.y = -grappleJumpBonus;
        trail.push_back({ position, 1.5f });
    }
    else if (IsInWater()) {
        velocity.y = -jumpForce;
    }
}

void Player::MoveAndCollide(float dt, const TerrainManager& terrain) {
    // Split fast movement into steps to reduce tunneling.
    const float maxStepDistance = collisionRadius * 0.5f;
    const float travelDistance = velocity.Length() * dt;
    int substepCount = static_cast<int>(std::ceil(travelDistance / maxStepDistance));
    if (substepCount < 1) substepCount = 1;

    // Cap the work per frame at extreme speeds.
    const int maxSubstepCount = 128;
    if (substepCount > maxSubstepCount) substepCount = maxSubstepCount;

    const float substepDt = dt / static_cast<float>(substepCount);
    isGrounded = false;

    for (int step = 0; step < substepCount; ++step) {
        position += velocity * substepDt;

        // Use up to two passes to resolve corners and multiple contacts.
        for (int iteration = 0; iteration < 2; ++iteration) {
            bool resolvedAnyCollision = false;

            for (const auto& obs : terrain.obstacles) {
                CollisionResult col = CheckPlayerCollision(position, collisionRadius, obs);
                if (!col.collided) continue;

                resolvedAnyCollision = true;
                position += col.pushVector;

                const bool landedOnTop = col.normal.y < -0.7f;
                if (landedOnTop) {
                    isGrounded = true;
                    if (velocity.y > 0.0f) velocity.y = 0.0f;
                }

                const float velocityAlongNormal = velocity.Dot(col.normal);
                if (velocityAlongNormal < 0.0f) {
                    // Give walls and ceilings a small bounce. Stop on floors.
                    const float bounce = landedOnTop ? 0.0f : 0.15f;
                    velocity -= col.normal * (1.0f + bounce) * velocityAlongNormal;

                    if (col.normal.y != 0.0f) velocity.x *= 0.98f;
                    if (col.normal.x != 0.0f) velocity.y *= 0.95f;
                }
            }

            if (!resolvedAnyCollision) break;
        }
    }

    RefreshGroundedState(terrain);
}

void Player::RefreshGroundedState(const TerrainManager& terrain) {
    // Probe below the player to detect contact without overlap.
    const float groundProbeDistance = 1.0f;
    const Vector2 probePosition = position + Vector2{ 0.0f, groundProbeDistance };

    for (const auto& obs : terrain.obstacles) {
        const CollisionResult col = CheckPlayerCollision(probePosition, collisionRadius, obs);
        if (col.collided && col.normal.y < -0.7f) {
            isGrounded = true;
            return;
        }
    }
}

void Player::Update(float dt, int horizontalInput, int verticalInput, bool jumpInput, bool reelInput, Water& water, float cameraLeft, const TerrainManager& terrain) {
    // Let the player sink after drowning.
    if (isDead) {
        const float deathGravity = 400.0f;
        velocity.y += deathGravity * dt;
        position += velocity * dt;
        const float SEA_BOTTOM_Y = 2000.0f;

        // Stop the body at the sea floor.
        if (position.y + 8.f > SEA_BOTTOM_Y) {
            position.y = SEA_BOTTOM_Y - 8.f;
        }

        water.EmitBlood(position, 2);
        return;
    }

    // Advance the flying hook and check for hits along its path.
    if (grappleState == GrappleState::Flying) {
        Vector2 prevHookPos = hookCurrentPos;

        Vector2 stepMove = hookVelocity * dt;
        hookCurrentPos += stepMove;
        currentFlyDist += stepMove.Length();

        // Keep the earliest hit in the current frame.
        bool hasHit = false;
        RaycastResult closestResult;
        closestResult.hit = false;
        closestResult.t = 1.0f;
        closestResult.hitPoint = hookCurrentPos;

        Vector2 stepDir = hookVelocity.Normalized();
        float stepLen = stepMove.Length();

        for (const auto& obs : terrain.obstacles) {
            RaycastResult res = RaycastToObstacle(prevHookPos, stepDir, stepLen, obs);
            if (res.hit) {
                float tFraction = (stepLen > 0.0001f) ? (res.t / stepLen) : 0.0f;
                if (tFraction < closestResult.t) {
                    closestResult = res;
                    closestResult.t = tFraction;
                    hasHit = true;
                }
            }

            // If the tip ends inside a block, try snapping it to the crossed edge.
            bool pointInside = (hookCurrentPos.x >= obs.Left() && hookCurrentPos.x <= obs.Right() &&
                hookCurrentPos.y >= obs.Top() && hookCurrentPos.y <= obs.Bottom());
            if (pointInside) {
                Vector2 snappedPos = hookCurrentPos;
                if (prevHookPos.x <= obs.Left() && hookCurrentPos.x >= obs.Left()) {
                    snappedPos.x = obs.Left();
                }
                else if (prevHookPos.x >= obs.Right() && hookCurrentPos.x <= obs.Right()) {
                    snappedPos.x = obs.Right();
                }
                else if (prevHookPos.y <= obs.Top() && hookCurrentPos.y >= obs.Top()) {
                    snappedPos.y = obs.Top();
                }
                else if (prevHookPos.y >= obs.Bottom() && hookCurrentPos.y <= obs.Bottom()) {
                    snappedPos.y = obs.Bottom();
                }

                if (!hasHit || 1.0f < closestResult.t) {
                    closestResult.hit = true;
                    closestResult.hitPoint = snappedPos;
                    closestResult.t = 1.0f;
                    hasHit = true;
                }
            }
        }

        
        if (hasHit) {
            // Pin the hook and start the rope at its current length.
            grappleState = GrappleState::Pinned;
            isGrappling = true;
            hookCurrentPos = closestResult.hitPoint;
            anchorPoint = hookCurrentPos;

            Vector2 toAnchor = anchorPoint - position;
            float actualDistance = toAnchor.Length();
            Vector2 pullDir = toAnchor.Normalized();

            // Redirect existing speed toward the anchor near the water surface.
            if (IsInWater() || position.y >= waterLineY - 30.0f) {
                float speed = velocity.Length();
                if (speed > 600.0f) speed = 600.0f;
                velocity = pullDir * speed;
            }
            else {
                if (velocity.y > 0.0f) {
                    velocity.y *= 0.15f;
                }
            }
            velocity += pullDir * launchPullForce * 1.6f;

            ropeLength = actualDistance;
            targetRopeLength = actualDistance;
        }
        else if (currentFlyDist >= MAX_GRAPPLE_RANGE) {
            grappleState = GrappleState::None;
            isGrappling = false;
        }
    }

    velocity.y += gravity * dt;

    const bool hasInput = (horizontalInput != 0) || (verticalInput != 0) || jumpInput || reelInput;
    const bool currentInWater = IsInWater();

    // Splash only when entering water while moving down.
    if (currentInWater && !wasInWater && velocity.y > 0.0f) {
        water.Splash(position.x, velocity.y, cameraLeft);
    }

    // Input or leaving the water restores full buoyancy.
    if (!currentInWater || hasInput) {
        noInputWaterTimer = 0.0f;
        buoyancyScale = 1.0f;
    }
    else {
        noInputWaterTimer += dt;

        if (noInputWaterTimer < buoyancyFadeStartTime) {
            buoyancyScale = 1.0f;
        }
        else if (noInputWaterTimer < drowningTime) {
            // Fade buoyancy linearly until the drowning time.
            const float fadeDuration = drowningTime - buoyancyFadeStartTime;
            const float fadeProgress = (noInputWaterTimer - buoyancyFadeStartTime) / fadeDuration;
            buoyancyScale = 1.0f - fadeProgress;

            if (rand() % 10 == 0) {
                water.EmitBlood(position, 1);
            }
        }
        else {
            isDead = true;
            velocity = { 0.0f, 60.0f };
            water.EmitBlood(position, 80);
            return;
        }
    }

    if (currentInWater) {
        // Scale buoyancy by the submerged share of the player diameter.
        float submergedDepth = position.y + collisionRadius - waterLineY;
        const float playerDiameter = collisionRadius * 2.0f;
        if (submergedDepth < 0.0f) submergedDepth = 0.0f;
        if (submergedDepth > playerDiameter) submergedDepth = playerDiameter;

        const float submergedRatio = submergedDepth / playerDiameter;
        const float currentBuoyancy = submergedRatio * buoyancyEngine * buoyancyScale;
        velocity.y -= currentBuoyancy * dt;
    }

    // Adjust the rope toward the requested length while attached.
    if (isGrappling) {
        if (ropeLength > targetRopeLength) {
            ropeLength -= autoPullSpeed * dt;
            if (ropeLength < targetRopeLength) ropeLength = targetRopeLength;
        }
        if (reelInput) {
            targetRopeLength -= reelSpeed * dt;
            if (targetRopeLength < 40.0f) targetRopeLength = 40.0f;
            ropeLength = targetRopeLength;
        }
        if (verticalInput == 1) {
            targetRopeLength += reelSpeed * dt;

            if (targetRopeLength > MAX_GRAPPLE_RANGE) {
                targetRopeLength = MAX_GRAPPLE_RANGE;
            }
        }

        if (ropeLength < targetRopeLength) {
            ropeLength += autoPullSpeed * dt;
            if (ropeLength > targetRopeLength) ropeLength = targetRopeLength;
        }
        else if (ropeLength > targetRopeLength) {
            ropeLength -= autoPullSpeed * dt;
            if (ropeLength < targetRopeLength) ropeLength = targetRopeLength;
        }

        if (ropeLength > MAX_GRAPPLE_RANGE) {
            ropeLength = MAX_GRAPPLE_RANGE;
        }

        Vector2 toPlayer = position - anchorPoint;
        float currentDist = toPlayer.Length();
        Vector2 ropeDir = toPlayer.Normalized();

        // Apply swing input tangent to the rope.
        Vector2 tangentDir = { ropeDir.y, -ropeDir.x };
        velocity += tangentDir * (static_cast<float>(horizontalInput) * swingForce * dt);

        // Pull the player within reach and remove outward speed when the rope is taut.
        if (currentDist >= ropeLength) {
            float vRope = velocity.Dot(ropeDir);
            if (vRope > 0.0f) {
                velocity = velocity - ropeDir * vRope;
            }
            position = anchorPoint + ropeDir * ropeLength;
        }
        if (currentInWater) {
            velocity *= (1.0f - waterDensity * dt);
        }
        else {
            velocity *= (1.0f - drag * dt);
        }

    }
    else {
        if (currentInWater) {
            // Apply drag proportional to the square of each speed component.
            velocity.x -= velocity.x * std::abs(velocity.x) * 0.002f * waterDensity * dt;
            velocity.y -= velocity.y * std::abs(velocity.y) * 0.005f * waterDensity * dt;

            velocity *= (1.0f - 2.0f * dt);

            if (horizontalInput != 0) {
                velocity.x += static_cast<float>(horizontalInput) * groundAcc * dt;
            }
        }
        else {
            if (horizontalInput != 0) {
                velocity.x += static_cast<float>(horizontalInput) * airControlForce * dt;
            }
            velocity.x *= (1.0f - drag * dt);
            velocity.y *= (1.0f - drag * dt);
        }
    }

    MoveAndCollide(dt, terrain);

    // Limit how deep a living player can sink.
    if (position.y > 650.0f) {
        position.y = 650.0f;
        if (velocity.y > 0.0f) velocity.y = 0.0f;
    }
    //if (position.x < 20.0f) { position.x = 20.0f;  velocity.x = -velocity.x * 0.5f; }
    //if (position.x > 780.0f) { position.x = 780.0f; velocity.x = -velocity.x * 0.5f; }

    wasInWater = currentInWater;

    // Leave a trail at high speed and fade older marks.
    if (velocity.Length() > 150.0f) {
        trail.push_back({ position, 1.0f });
    }
    for (size_t i = 0; i < trail.size(); ) {
        trail[i].alpha -= 3.0f * dt;
        if (trail[i].alpha <= 0.0f) {
            trail.erase(trail.begin() + i);
        }
        else {
            i++;
        }
    }
}

void Player::Draw(const Camera& camera, const Vector2& mouseWorldPos, const TerrainManager& terrain) {
    // Preview the nearest reachable anchor in the aim direction.
    if (!isDead && !isGrappling) {
        Vector2 toMouse = mouseWorldPos - position;
        float distToMouse = toMouse.Length();

        if (distToMouse > 10.0f) {
            Vector2 rayDir = toMouse.Normalized();
            const float MAX_GRAPPLE_RANGE = 600.0f; // Match the hook range.

            bool foundClosestHit = false;
            RaycastResult closestResult;
            closestResult.t = MAX_GRAPPLE_RANGE;

            for (const auto& obs : terrain.obstacles) {
                RaycastResult res = RaycastToObstacle(position, rayDir, MAX_GRAPPLE_RANGE, obs);
                if (res.hit && res.t < closestResult.t) {
                    closestResult = res;
                    foundClosestHit = true;
                }
            }

            Vector2 lineStartWorld = position;
            Vector2 lineEndWorld = foundClosestHit ? closestResult.hitPoint : (position + rayDir * MAX_GRAPPLE_RANGE);

            Vector2 sStart = camera.WorldToScreen(lineStartWorld);
            Vector2 sEnd = camera.WorldToScreen(lineEndWorld);

            Vector2 screenDelta = sEnd - sStart;
            float screenDist = screenDelta.Length();
            Vector2 screenDir = screenDelta.Normalized();

            const float dotStep = 15.0f;

            // Teal marks a reachable anchor. Gray marks empty space.
            unsigned int lineColor = foundClosestHit ? GetColor(0, 255, 200) : GetColor(120, 120, 120);

            SetDrawBlendMode(DX_BLENDMODE_ALPHA, foundClosestHit ? 180 : 80);

            for (float d = 0.0f; d < screenDist; d += dotStep) {
                Vector2 dotPos = sStart + screenDir * d;
                int radius = foundClosestHit ? static_cast<int>(3.0f * (1.0f - (d / screenDist * 0.5f))) : 2;
                if (radius < 1) radius = 1;

                DrawCircle(static_cast<int>(dotPos.x), static_cast<int>(dotPos.y), radius, lineColor, TRUE);
            }

            if (foundClosestHit) {
                int targetX = static_cast<int>(sEnd.x);
                int targetY = static_cast<int>(sEnd.y);
                DrawCircle(targetX, targetY, 6, lineColor, FALSE);
                DrawCircle(targetX, targetY, 2, GetColor(255, 255, 255), TRUE);
            }

            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        }
    }

    // Draw the fading movement trail.
    for (const auto& t : trail) {
        Vector2 sTrailPos = camera.WorldToScreen(t.pos);

        int alphaVal = static_cast<int>(t.alpha * 120);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alphaVal);
        DrawCircle(static_cast<int>(sTrailPos.x), static_cast<int>(sTrailPos.y), 12, GetColor(0, 255, 255), TRUE);
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    // Draw the rope to the moving tip or fixed anchor.
    if (grappleState == GrappleState::Flying || grappleState == GrappleState::Pinned) {
        Vector2 sPlayerPos = camera.WorldToScreen(position);

        Vector2 targetTip = (grappleState == GrappleState::Flying) ? hookCurrentPos : anchorPoint;
        Vector2 sAnchorPos = camera.WorldToScreen(targetTip);

        int pX = static_cast<int>(sPlayerPos.x);
        int pY = static_cast<int>(sPlayerPos.y);
        int aX = static_cast<int>(sAnchorPos.x);
        int aY = static_cast<int>(sAnchorPos.y);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, (grappleState == GrappleState::Flying) ? 160 : 100);
        unsigned int ropeColor = (grappleState == GrappleState::Flying) ? GetColor(0, 180, 255) : GetColor(0, 255, 128);
        DrawLine(aX, aY, pX, pY, ropeColor, 5);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        DrawLine(aX, aY, pX, pY, GetColor(255, 255, 255), 2);

        if (grappleState == GrappleState::Flying) {
            // Layer translucent circles for the flying tip glow.
            unsigned int glowColor = GetColor(0, 230, 255);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 50);
            DrawCircle(aX, aY, 14, glowColor, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 100);
            DrawCircle(aX, aY, 8, glowColor, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
            DrawCircle(aX, aY, 4, glowColor, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            DrawCircle(aX, aY, 2, GetColor(255, 255, 255), TRUE);
        }
        else {
            // Mark the fixed anchor in red.
            unsigned int pinColor = GetColor(255, 50, 50);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 100);
            DrawCircle(aX, aY, 10, pinColor, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            DrawCircle(aX, aY, 5, pinColor, TRUE);
            DrawCircle(aX, aY, 2, GetColor(255, 255, 255), TRUE);
        }
    }

    // Draw the player.
    Vector2 sPlayerPos = camera.WorldToScreen(position);
    int pX = static_cast<int>(sPlayerPos.x);
    int pY = static_cast<int>(sPlayerPos.y);

    float speed = velocity.Length();
    Vector2 moveDir = velocity.Normalized();

    DrawCircle(pX, pY, 16, GetColor(200, 200, 200), TRUE);
    DrawCircle(pX, pY, 14, GetColor(255, 255, 255), TRUE);
    DrawCircle(pX, pY, 10, GetColor(0, 160, 255), TRUE);
    DrawCircle(pX, pY, 7, GetColor(190, 255, 255), TRUE);
    DrawCircle(pX, pY, 3, GetColor(70, 75, 80), TRUE);
    DrawPixel(pX, pY, GetColor(255, 255, 255));
}
