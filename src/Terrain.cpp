#include "Terrain.hpp"
#include "Camera.hpp"
#include "DxLib.h"

#include <cstdint>
#include <random>

namespace {
    std::uint32_t MakeChunkSeed(int chunkX, int chunkY) {
        // Mix both coordinate bit patterns with unsigned arithmetic.
        std::uint64_t value =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(chunkX)) << 32) |
            static_cast<std::uint32_t>(chunkY);

        value += 0x9e3779b97f4a7c15ULL;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        value ^= value >> 31;

        return static_cast<std::uint32_t>(value ^ (value >> 32));
    }

    int RandomInt(std::mt19937& engine, int minimum, int maximum) {
        return std::uniform_int_distribution<int>(minimum, maximum)(engine);
    }

    std::mt19937& EffectRandomEngine() {
        // Use a separate random engine for visual effects.
        static std::mt19937 engine(std::random_device{}());
        return engine;
    }
}

void TerrainManager::SpawnExplosion(Vector2 spawnPos, unsigned int coreColor) {
    const int particleCount = 16;
    std::mt19937& randomEngine = EffectRandomEngine();

    for (int i = 0; i < particleCount; i++) {
        // Pick a random direction and speed for each particle.
        float angle = static_cast<float>(RandomInt(randomEngine, 0, 359)) * (3.1415926f / 180.0f);
        float speed = 150.0f + static_cast<float>(RandomInt(randomEngine, 0, 249));

        Vector2 pVel = { std::cos(angle) * speed, std::sin(angle) * speed };

        particles.push_back({ spawnPos, pVel, 1.0f, coreColor });
    }
}

void TerrainManager::Init() {
    lastGeneratedX = 300.0f;

    obstacles.clear();
    cores.clear();
    particles.clear();
    loadedChunks.clear();

    // Keep the spawn platform loaded when chunks unload.
    obstacles.push_back({
        200.0f, 500.0f, 400.0f, 40.0f,
        GetColor(110, 110, 120),
        0, 0, true
    });
}

void TerrainManager::UpdateAllDirections(Vector2 cameraPos) {
    // Find the camera's chunk, including negative coordinates.
    int currentCX = static_cast<int>(std::floor(cameraPos.x / CHUNK_SIZE));
    int currentCY = static_cast<int>(std::floor(cameraPos.y / CHUNK_SIZE));

    // Load missing chunks around the camera.
    for (int cx = currentCX - LOAD_RADIUS; cx <= currentCX + LOAD_RADIUS; ++cx) {
        for (int cy = currentCY - LOAD_RADIUS; cy <= currentCY + LOAD_RADIUS; ++cy) {

            if (loadedChunks.find({ cx, cy }) == loadedChunks.end()) {
                GenerateChunk(cx, cy);
                loadedChunks.insert({ cx, cy });
            }
        }
    }

    // Forget distant chunks so they can be generated again on return.
    for (auto it = loadedChunks.begin(); it != loadedChunks.end(); ) {
        const int distanceX = std::abs(it->first - currentCX);
        const int distanceY = std::abs(it->second - currentCY);

        if (distanceX > UNLOAD_RADIUS || distanceY > UNLOAD_RADIUS) {
            it = loadedChunks.erase(it);
        }
        else {
            ++it;
        }
    }

    // Remove obstacles owned by unloaded chunks.
    for (auto it = obstacles.begin(); it != obstacles.end(); ) {
        const bool ownerIsLoaded = loadedChunks.find({ it->ownerChunkX, it->ownerChunkY }) != loadedChunks.end();

        if (!it->isPersistent && !ownerIsLoaded) {
            it = obstacles.erase(it);
        }
        else {
            ++it;
        }
    }

    // Remove collected cores and cores in unloaded chunks.
    for (auto it = cores.begin(); it != cores.end(); ) {
        const bool ownerIsLoaded = loadedChunks.find({ it->ownerChunkX, it->ownerChunkY }) != loadedChunks.end();

        if (!ownerIsLoaded || it->isCollected) {
            it = cores.erase(it);
        }
        else {
            ++it;
        }
    }

    // Advance and fade pickup particles with a fixed time step.
    const float dt = 0.008f;
    for (auto it = particles.begin(); it != particles.end(); ) {
        it->pos += it->velocity * dt;

        it->velocity *= (1.0f - 2.5f * dt);

        it->alpha -= 2.5f * dt;

        if (it->alpha <= 0.0f) {
            it = particles.erase(it);
        }
        else {
            it++;
        }
    }
}

void TerrainManager::GenerateChunk(int cx, int cy) {
    float chunkStartY = cy * CHUNK_SIZE;
    const float WATER_SURFACE_Y = 550.0f;

    // Skip chunks that start below the waterline.
    if (chunkStartY >= WATER_SURFACE_Y) {
        return;
    }

    // Seed each chunk from its coordinates so it regenerates the same way.
    std::mt19937 randomEngine(MakeChunkSeed(cx, cy));

    float chunkStartX = cx * CHUNK_SIZE;

    // Leave some chunks empty.
    if (RandomInt(randomEngine, 0, 99) < 30) return;

    int count = RandomInt(randomEngine, 1, 2);

    for (int i = 0; i < count; i++) {
        float w = 80.0f + static_cast<float>(RandomInt(randomEngine, 0, 139));
        float h = 30.0f + static_cast<float>(RandomInt(randomEngine, 0, 49));

        // Keep each block within its chunk.
        const int maxOffsetX = static_cast<int>(CHUNK_SIZE - w) - 1;
        const int maxOffsetY = static_cast<int>(CHUNK_SIZE - h) - 1;
        float rx = chunkStartX + static_cast<float>(RandomInt(randomEngine, 0, maxOffsetX));
        float ry = chunkStartY + static_cast<float>(RandomInt(randomEngine, 0, maxOffsetY));

        // Trim blocks at the waterline and skip very short blocks.
        if (ry + h > WATER_SURFACE_Y) {
            h = WATER_SURFACE_Y - ry - 2.0f;
            if (h < 15.0f) continue;
        }

        unsigned int blockColor = (cy < 0) ? GetColor(140, 150, 160) : GetColor(110, 115, 100);

        obstacles.push_back({ rx, ry, w, h, blockColor, cx, cy, false });

        // Roll for a core above or below each block.
        if (RandomInt(randomEngine, 0, 99) < 60) { // 60% spawn chance.
            float coreX = rx + w * 0.5f;
            float coreY = 0.0f;

            if (RandomInt(randomEngine, 0, 1) == 0) {
                coreY = ry + h + 60.0f;
            }
            else {
                coreY = ry - 50.0f;
            }

            // Keep cores clear of the water surface.
            if (coreY < WATER_SURFACE_Y - 20.0f) {
                unsigned int coreColor = GetColor(250, 0, 0);
                cores.push_back({
                    coreX, coreY, 12.0f, coreColor, false,
                    static_cast<float>(RandomInt(randomEngine, 0, 99)) * 0.01f,
                    cx, cy
                });
            }
        }
    }
}

void TerrainManager::Draw(const Camera& camera) {
    for (const auto& obs : obstacles) {
        Vector2 topLeft = camera.WorldToScreen({ obs.x, obs.y });
        Vector2 bottomRight = camera.WorldToScreen({ obs.x + obs.width, obs.y + obs.height });

        int x1 = static_cast<int>(topLeft.x);
        int y1 = static_cast<int>(topLeft.y);
        int x2 = static_cast<int>(bottomRight.x);
        int y2 = static_cast<int>(bottomRight.y);

        // Layer outlines to give each block a glow.
        unsigned int glowColor = GetColor(0, 240, 210);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 35);
        DrawBox(x1 - 6, y1 - 6, x2 + 6, y2 + 6, glowColor, FALSE);
        DrawBox(x1 - 5, y1 - 5, x2 + 5, y2 + 5, glowColor, FALSE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 75);
        DrawBox(x1 - 3, y1 - 3, x2 + 3, y2 + 3, glowColor, FALSE);
        DrawBox(x1 - 2, y1 - 2, x2 + 2, y2 + 2, glowColor, FALSE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 160);
        DrawBox(x1 - 1, y1 - 1, x2 + 1, y2 + 1, glowColor, FALSE);

        unsigned int coreColor = GetColor(220, 255, 250);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
        DrawBox(x1, y1, x2, y2, coreColor, FALSE);
    }

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    // Draw each core with a pulsing glow.
    for (auto& core : cores) {
        if (core.isCollected) continue;

        core.pulseTimer += 0.05f;
        float pulseScale = 1.0f + std::sin(core.pulseTimer) * 0.25f;

        Vector2 sCorePos = camera.WorldToScreen({ core.x, core.y });
        int cx = static_cast<int>(sCorePos.x);
        int cy = static_cast<int>(sCorePos.y);
        int baseR = static_cast<int>(core.radius);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 80);
        DrawCircle(cx, cy, static_cast<int>(baseR * pulseScale * 1.8f), core.color, TRUE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
        DrawCircle(cx, cy, static_cast<int>(baseR * pulseScale), core.color, FALSE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
        DrawCircle(cx, cy, 4, GetColor(255, 205, 205), TRUE);
        DrawCircle(cx, cy, 1, GetColor(255, 255, 255), TRUE);
    }

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    // Draw pickup particles.
    for (const auto& p : particles) {
        Vector2 sPartPos = camera.WorldToScreen(p.pos);
        int px = static_cast<int>(sPartPos.x);
        int py = static_cast<int>(sPartPos.y);

        int alphaVal = static_cast<int>(p.alpha * 255);
        if (alphaVal < 0) alphaVal = 0;

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alphaVal / 3);
        DrawCircle(px, py, 8, p.color, TRUE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alphaVal);
        DrawCircle(px, py, 3, GetColor(255, 255, 255), TRUE);
        DrawCircle(px, py, 4, p.color, FALSE);
    }
}
