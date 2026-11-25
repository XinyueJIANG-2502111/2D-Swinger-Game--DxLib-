#include "ControlsGuide.hpp"
#include "HudPanel.hpp"
#include <algorithm>
#include <array>
#include <cstring>

namespace {
    struct ControlRow {
        const char* input;
        const char* action;
    };

    constexpr std::array<ControlRow, 8> controls{{
        { "A/D, Left/Right", "Move/swing" },
        { "W, Up",           "Reel in" },
        { "S, Down",         "Let out" },
        { "Space",           "Jump" },
        { "Left mouse",      "Fire hook" },
        { "Right mouse",     "Unhook" },
        { "R",               "Respawn*" },
        { "Esc",             "Quit*" },
    }};
}

void ControlsGuide::Update(bool tabDown) {
    // Toggle once per press, even if Tab is held across several frames.
    if (tabDown && !tabWasDown) {
        expanded = !expanded;
    }
    tabWasDown = tabDown;
}

void ControlsGuide::Draw(int left, int top, int collapsedWidth, int collapsedHeight) const {
    constexpr int padding = 12;
    const int fontHeight = GetFontSize();
    const auto white = GetColor(255, 255, 255);

    // Keep the shortcut visible in a compact panel when the guide is collapsed.
    if (!expanded) {
        DrawHudPanel(left, top, left + collapsedWidth, top + collapsedHeight);
        DrawString(left + padding, top + (collapsedHeight - fontHeight) / 2, "[Tab] Show controls", white);
        return;
    }

    constexpr char title[] = "CONTROLS  [Tab: Collapse]";
    constexpr char footer[] = "* After game over";
    constexpr int minimumWidth = 310;
    constexpr int columnGap = 20;
    const int rowHeight = fontHeight + 4;
    const int rowsTop = top + 9 + fontHeight + 10;
    const int footerTop = rowsTop + static_cast<int>(controls.size()) * rowHeight + 5;

    // Measure both columns with the active font so the text never shares the same space.
    const auto textWidth = [](const char* text) {
        return GetDrawStringWidth(text, static_cast<int>(std::strlen(text)));
    };
    int inputWidth = 0;
    int actionWidth = 0;
    for (const auto& row : controls) {
        inputWidth = (std::max)(inputWidth, textWidth(row.input));
        actionWidth = (std::max)(actionWidth, textWidth(row.action));
    }
    const int rowsWidth = inputWidth + columnGap + actionWidth;
    const int contentWidth = (std::max)({ rowsWidth, textWidth(title), textWidth(footer) });
    const int panelWidth = (std::max)(minimumWidth, contentWidth + padding * 2);
    const int actionLeft = left + padding + inputWidth + columnGap;

    DrawHudPanel(left, top, left + panelWidth, footerTop + fontHeight + 8);
    DrawString(left + padding, top + 9, title, white);

    int y = rowsTop;
    for (const auto& row : controls) {
        DrawString(left + padding, y, row.input, white);
        DrawString(actionLeft, y, row.action, white);
        y += rowHeight;
    }
    DrawString(left + padding, footerTop, footer, white);
}
