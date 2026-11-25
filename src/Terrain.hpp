#pragma once

#include "Vector.hpp"

#include <vector>
#include <set>
#include <cmath>
#include <cstdlib>


class Camera;

// Axis-aligned terrain block.
struct Obstacle {
    float x, y; // Top-left world position.
    float width, height;
    unsigned int color;

    // Procedural blocks unload with their owner chunk.
    int ownerChunkX = 0;
    int ownerChunkY = 0;
    // Persistent blocks survive chunk unloading.
    bool isPersistent = false;

    float Left() const { return x; }
    float Right() const { return x + width; }
    float Top() const { return y; }
    float Bottom() const { return y + height; }
};

// First intersection along the ray.
struct RaycastResult {
    bool hit;
    Vector2 hitPoint;
    float t; // Distance along a normalized ray.
};

// Circle against terrain contact.
struct CollisionResult {
    bool collided;
    Vector2 pushVector; // Displacement needed to clear the block.
    Vector2 normal; // Outward contact normal.
};

// Collectible core.
struct CoreItem {
    float x, y; // Center in world coordinates.
    float radius;
    unsigned int color;
    bool isCollected;
    float pulseTimer; // Phase of the pulse animation.

    // A core unloads with the chunk that spawned it.
    int ownerChunkX = 0;
    int ownerChunkY = 0;
};

struct EffectParticle {
    Vector2 pos;
    Vector2 velocity;
    float alpha;
    unsigned int color;
};

// Intersect the ray with the block on both axes.
inline RaycastResult RaycastToObstacle(const Vector2& rayStart, const Vector2& rayDir, float maxDist, const Obstacle& obs) {
    RaycastResult result = { false, {0,0}, maxDist };

    float tXMin = (obs.Left() - rayStart.x) / rayDir.x;
    float tXMax = (obs.Right() - rayStart.x) / rayDir.x;
    if (tXMin > tXMax) std::swap(tXMin, tXMax);

    float tYMin = (obs.Top() - rayStart.y) / rayDir.y;
    float tYMax = (obs.Bottom() - rayStart.y) / rayDir.y;
    if (tYMin > tYMax) std::swap(tYMin, tYMax);

    // The ray must be inside both axis ranges at the same point.
    if (tXMin > tYMax || tYMin > tXMax) return result;

    float tNear = (std::max)(tXMin, tYMin);
    float tFar = (std::min)(tXMax, tYMax);

    if (tFar < 0.0f) return result;

    if (tNear > maxDist || tNear < 0.0f) return result;

    result.hit = true;
    result.t = tNear;
    result.hitPoint = rayStart + rayDir * tNear;
    return result;
}

// Test a circular player against an axis-aligned block.
inline CollisionResult CheckPlayerCollision(const Vector2& playerPos, float radius, const Obstacle& obs) {
    CollisionResult result = { false, {0,0}, {0,0} };

    // Clamp the center to the block to find its closest point.
    float closestX = (std::max)(obs.Left(), (std::min)(playerPos.x, obs.Right()));
    float closestY = (std::max)(obs.Top(), (std::min)(playerPos.y, obs.Bottom()));

    Vector2 diff = playerPos - Vector2{ closestX, closestY };
    float distance = diff.Length();

    if (distance < radius && distance > 0.001f) {
        result.collided = true;
        float overlap = radius - distance;
        result.normal = diff.Normalized();
        result.pushVector = result.normal * overlap;
    }
    else if (distance == 0.0f) {
        // If the center is inside or on the block, push the circle out through the nearest edge.
        result.collided = true;
        float dl = playerPos.x - obs.Left();
        float dr = obs.Right() - playerPos.x;
        float dt = playerPos.y - obs.Top();
        float db = obs.Bottom() - playerPos.y;
        float minVal = (std::min)({ dl, dr, dt, db });

        if (minVal == dl) { result.pushVector = { -(radius + dl), 0 }; result.normal = { -1, 0 }; }
        else if (minVal == dr) { result.pushVector = { radius + dr, 0 };  result.normal = { 1, 0 }; }
        else if (minVal == dt) { result.pushVector = { 0, -(radius + dt) }; result.normal = { 0, -1 }; }
        else { result.pushVector = { 0, radius + db };  result.normal = { 0, 1 }; }
    }

    return result;
}



class TerrainManager {
public:
    const float CHUNK_SIZE = 400.0f; // Chunk width and height in pixels.

    std::set<std::pair<int, int>> loadedChunks;

public:
    std::vector<Obstacle> obstacles;
    std::vector<CoreItem> cores;

    float lastGeneratedX = 300.0f;
    const float CHUNK_WIDTH = 400.0f;

    std::vector<EffectParticle> particles;

    void SpawnExplosion(Vector2 spawnPos, unsigned int coreColor);

    void Init();

    // Load nearby chunks and unload distant ones.
    void UpdateAllDirections(Vector2 cameraPos);

private:
    // Unload farther away to avoid repeated loading at chunk borders.
    static constexpr int LOAD_RADIUS = 2;
    static constexpr int UNLOAD_RADIUS = 4;

    void GenerateChunk(int cx, int cy);

public:
    void Draw(const Camera& camera);
};
