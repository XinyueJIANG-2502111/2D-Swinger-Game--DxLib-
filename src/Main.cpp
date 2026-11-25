#include "DxLib.h"
#include "Vector.hpp"
#include "Camera.hpp"
#include "Player.hpp"
#include "Water.hpp"
#include "Terrain.hpp"
#include "ControlsGuide.hpp"
#include "HudPanel.hpp"
#include <cmath>
#include <vector>

int playerScore = 0;
const float PLAYER_COLLISION_RADIUS = 16.0f;

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    // Set up the DxLib window.
    ChangeWindowMode(TRUE);
    SetGraphMode(800, 600, 32);
    SetWindowText("test");

    if (DxLib_Init() == -1) return -1;
    // Draw to the back buffer before presenting the frame.
    SetDrawScreen(DX_SCREEN_BACK);

    Player player;
    Water water;
    Camera camera;
    TerrainManager terrain;
    ControlsGuide controlsGuide;

    player.Reset({ 400.0f, 400.0f });
    terrain.Init();

    LONGLONG prevTime = GetNowHiPerformanceCount();

    while (ProcessMessage() == 0 && ClearDrawScreen() == 0) {
        // Convert the timer from microseconds to seconds.
        LONGLONG currTime = GetNowHiPerformanceCount();
        float dt = static_cast<float>(currTime - prevTime) / 1000000.0f;
        prevTime = currTime;
        // Cap long frames to keep physics steps stable.
        if (dt > 0.05f) dt = 0.05f;

        // Read player controls.
        controlsGuide.Update(CheckHitKey(KEY_INPUT_TAB) > 0);

        int horizontalInput = 0;
        if (CheckHitKey(KEY_INPUT_A) || CheckHitKey(KEY_INPUT_LEFT))  horizontalInput = -1;
        if (CheckHitKey(KEY_INPUT_D) || CheckHitKey(KEY_INPUT_RIGHT)) horizontalInput = 1;

        int verticalInput = 0;
        if (CheckHitKey(KEY_INPUT_S) || CheckHitKey(KEY_INPUT_DOWN)) verticalInput = 1;

        bool jumpInput = false;
        if (CheckHitKey(KEY_INPUT_SPACE)) {
            jumpInput = true;
            player.Jump();
        }

        bool reelInput = (CheckHitKey(KEY_INPUT_W) || CheckHitKey(KEY_INPUT_UP));

        int mouseX, mouseY;
        GetMousePoint(&mouseX, &mouseY);

        // Undo the camera offset to aim in world coordinates.
        Vector2 mouseWorldPos = {
            static_cast<float>(mouseX) + camera.position.x - 400.0f,
            static_cast<float>(mouseY) + camera.position.y - 300.0f
        };

        // Left click fires the hook and counts as player activity.
        if ((GetMouseInput() & MOUSE_INPUT_LEFT) != 0) {
            jumpInput = true;

            if (player.grappleState == Player::GrappleState::None) {
                player.LaunchGrapple(mouseWorldPos, terrain);
            }
        }

        // Right click releases the hook.
        if ((GetMouseInput() & MOUSE_INPUT_RIGHT) != 0) {
            player.ReleaseGrapple();
        }

        // R restarts the game after drowning.
        if (player.isDead && CheckHitKey(KEY_INPUT_R)) {
            // Reset the player, water, camera, terrain, and score.
            player.Reset({ 400.0f, 300.0f });
            water.Reset();
            playerScore = 0;
            camera.Init();
            terrain.Init();
        }

        // Esc exits from the game-over screen.
        if (player.isDead && CheckHitKey(KEY_INPUT_ESCAPE)) {
            break;
        }

        // Stream terrain around the camera.
        terrain.UpdateAllDirections(camera.position);

        // Collect cores when their circles overlap the player.
        if (!player.isDead) {
            for (auto& core : terrain.cores) {
                if (core.isCollected) continue;

                float dx = player.position.x - core.x;
                float dy = player.position.y - core.y;
                float distance = std::sqrt(dx * dx + dy * dy);

                if (distance < (PLAYER_COLLISION_RADIUS + core.radius)) {

                    core.isCollected = true;
                    playerScore += 1;

                    terrain.SpawnExplosion({ core.x, core.y }, core.color);
                    player.trail.push_back({ player.position, 2.0f });
                }
            }
        }

        // Update the player before moving the camera.
        water.Update(dt);
        float cameraLeft = camera.position.x - 400.0f;
        player.Update(dt, horizontalInput,verticalInput, jumpInput, reelInput, water, cameraLeft, terrain);
        camera.Follow(player.position, dt);

        // Draw a world-aligned grid down to the waterline.
        const int gridSpacing = 40;

        float worldLeft = camera.position.x - 400.0f;
        float worldRight = camera.position.x + 400.0f;
        float worldTop = camera.position.y - 300.0f;
        float worldBottom = camera.position.y + 300.0f;

        // Snap the first grid lines to world coordinates.
        int startGridX = static_cast<int>(floorf(worldLeft / gridSpacing)) * gridSpacing;
        int startGridY = static_cast<int>(floorf(worldTop / gridSpacing)) * gridSpacing;

        for (int x = startGridX; x <= worldRight; x += gridSpacing) {
            Vector2 sTop = camera.WorldToScreen({ static_cast<float>(x), worldTop });
            Vector2 sBottom = camera.WorldToScreen({ static_cast<float>(x), 550.0f });

            if (sBottom.y < sTop.y) sBottom.y = sTop.y;

            DrawLine(static_cast<int>(sTop.x), static_cast<int>(sTop.y),
                static_cast<int>(sBottom.x), static_cast<int>(sBottom.y),
                GetColor(30, 30, 45));
        }

        for (int y = startGridY; y <= 550; y += gridSpacing) {
            if (y > 550) continue;

            Vector2 sLeft = camera.WorldToScreen({ worldLeft,  static_cast<float>(y) });
            Vector2 sRight = camera.WorldToScreen({ worldRight, static_cast<float>(y) });

            DrawLine(static_cast<int>(sLeft.x), static_cast<int>(sLeft.y),
                static_cast<int>(sRight.x), static_cast<int>(sRight.y),
                GetColor(30, 30, 45));
        }

        // Draw water last so it overlays the player and terrain.
        terrain.Draw(camera);
        player.Draw(camera, mouseWorldPos, terrain);
        water.Draw(camera);

        // Draw the game-over overlay.
        if (player.isDead) {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
            DrawBox(0, 0, 800, 600, GetColor(255, 255, 255), TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

            DrawString(340, 300, "Press [R] to Respawn", GetColor(255, 50, 50));
            DrawString(340, 330, "Press [Esc] to Quit", GetColor(255, 50, 50));
        }

        // Keep the HUD readable above the game-over overlay.
        // ゲームオーバー画面でも読めるよう、HUD はオーバーレイの上に描画する。
        constexpr int scoreLeft = 15;
        constexpr int scoreTop = 15;
        constexpr int scoreRight = 250;
        constexpr int scoreBottom = 50;
        DrawHudPanel(scoreLeft, scoreTop, scoreRight, scoreBottom);
        DrawFormatString(scoreLeft + 15, scoreTop + 9, GetColor(255, 255, 255), "CORE COLLECTED: %06d", playerScore);
        controlsGuide.Draw(scoreLeft, scoreBottom + 6, scoreRight - scoreLeft, scoreBottom - scoreTop);

        // Optional debug display.
        //DrawString(20, 20, "[CAMERA TEST] Mouse Left: Shoot Hook anywhere | Mouse Right: Release Hook", GetColor(255, 255, 255));
        //DrawString(20, 40, "              A / D: Add Swing Force | W: Reel In | SPACE: Jump", GetColor(255, 255, 255));
        
        //DrawFormatString(20, 60, GetColor(0, 255, 255), "Player World Pos: X:%.1f, Y:%.1f", player.position.x, player.position.y);
        //DrawFormatString(20, 80, GetColor(0, 255, 255), "Camera World Pos: X:%.1f, Y:%.1f", camera.position.x, camera.position.y);
        //DrawFormatString(20, 100, GetColor(255, 255, 0), "Player Speed    : %.2f px/s", player.velocity.Length());
        //DrawFormatString(20, 120, GetColor(255, 255, 0), "Rope Length     : %.2f px", player.isGrappling ? player.ropeLength : 0.0f);

        /*DrawFormatString(20, 150, GetColor(255, 255, 255),
            "Obstacles: %d | Loaded Chunks: %d | Cam: (%.1f, %.1f)",
            terrain.obstacles.size(), terrain.loadedChunks.size(), camera.position.x, camera.position.y);*/

        ScreenFlip();
    }

    DxLib_End();
    return 0;
}
