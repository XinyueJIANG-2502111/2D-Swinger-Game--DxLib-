#pragma once

#include "DxLib.h"

inline void DrawHudPanel(int left, int top, int right, int bottom) {
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 120);
    DrawBox(left, top, right, bottom, GetColor(0, 0, 0), TRUE);
    DrawBox(left, top, right, bottom, GetColor(0, 240, 210), FALSE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}
