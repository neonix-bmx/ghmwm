#pragma once

#include <cstdint>
#include <vector>

struct Color { uint8_t r, g, b, a; };

struct Window
{
    int x, y, w, h;
    Color bg;
    Color border;
    Color title;
    bool minimized = false;
    bool fullscreen = false;
    int savedX = 0, savedY = 0, savedW = 0, savedH = 0;
};

class WindowManager
{
public:
    enum class SnapRegion { None, Left, Right, Top, Bottom, Full, TopLeft, TopRight, BottomLeft, BottomRight, LeftThird, CenterThird, RightThird };

    void init(int resX, int resY);
    void setResolution(int resX, int resY);

    // Handle mouse state; left/right current and previous
    void handleMouse(int cx, int cy, bool left, bool right, bool prevLeft, bool prevRight);

    // Keyboard-driven actions
    void snapFocused(SnapRegion region);
    void toggleMinimizeFocused();
    void toggleFullscreenFocused();

    // Draw windows and context menu
    void draw(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h);

private:
    void drawWindow(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h, const Window& win) const;
    void drawMenu(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h) const;
    void bringToFront(int idx);

    void toggleMinimize(int idx);
    void toggleFullscreen(int idx);
    void applySnap(Window& w, SnapRegion region);
    void createDefaultWindow(int x, int y);

    int resX_ = 0;
    int resY_ = 0;
    std::vector<Window> windows_;

    int activeWin_ = -1;
    int focused_ = -1;
    bool dragging_ = false;
    bool resizing_ = false;
    int dragOffX_ = 0, dragOffY_ = 0;
    int pressX_ = 0, pressY_ = 0;
    int startW_ = 0, startH_ = 0;
    int dragRestoreX_ = 0, dragRestoreY_ = 0, dragRestoreW_ = 0, dragRestoreH_ = 0;
    bool unSnapApplied_ = false;

    struct Menu
    {
        bool open = false;
        int x = 0, y = 0;
        int target = -1; // -1 means desktop context menu
        int width = 160;
        int itemH = 22;
    } menu_;

    // Tiling/snap state
    SnapRegion snapRegion_ = SnapRegion::None;
    SnapRegion lastSnapRegion_ = SnapRegion::None;
};
