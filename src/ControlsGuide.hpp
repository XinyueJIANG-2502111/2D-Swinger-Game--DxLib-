#pragma once

class ControlsGuide {
public:
    void Update(bool tabDown);
    void Draw(int left, int top, int collapsedWidth, int collapsedHeight) const;

private:
    bool expanded = true;
    bool tabWasDown = false;
};
