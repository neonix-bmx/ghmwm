#include "window_manager.hpp"

#include <algorithm>

namespace
{
inline void fillRect(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h,
                     uint32_t x0, uint32_t y0, uint32_t rw, uint32_t rh, Color c)
{
    uint32_t x1 = (x0 + rw > w) ? w : x0 + rw;
    uint32_t y1 = (y0 + rh > h) ? h : y0 + rh;
    for(uint32_t y = y0; y < y1; ++y)
    {
        uint8_t* row = fb + y * pitch + x0 * 4;
        for(uint32_t x = x0; x < x1; ++x)
        {
            row[0] = c.b; row[1] = c.g; row[2] = c.r; row[3] = c.a;
            row += 4;
        }
    }
}
}

void WindowManager::init(int resX, int resY)
{
    resX_ = resX;
    resY_ = resY;
    windows_.clear();
    createDefaultWindow(60, 50);
    createDefaultWindow(resX / 2, resY / 2);
    focused_ = (int)windows_.size() - 1;
}

void WindowManager::createDefaultWindow(int x, int y)
{
    int w = std::max(160, resX_ / 3);
    int h = std::max(120, resY_ / 3);
    if(x + w > resX_) x = std::max(0, resX_ - w);
    if(y + h > resY_) y = std::max(0, resY_ - h);
    Window win{};
    win.x = x; win.y = y; win.w = w; win.h = h;
    win.bg = {220, 230, 240, 255};
    win.border = {50, 50, 80, 255};
    win.title = {80, 110, 200, 255};
    windows_.push_back(win);
}

void WindowManager::setResolution(int resX, int resY)
{
    resX_ = resX;
    resY_ = resY;
}

void WindowManager::bringToFront(int idx)
{
    if(idx < 0 || idx >= (int) windows_.size()) return;
    auto w = windows_[idx];
    windows_.erase(windows_.begin() + idx);
    windows_.push_back(w);
}

void WindowManager::toggleMinimize(int idx)
{
    if(idx < 0 || idx >= (int) windows_.size()) return;
    windows_[idx].minimized = !windows_[idx].minimized;
}

void WindowManager::toggleFullscreen(int idx)
{
    if(idx < 0 || idx >= (int) windows_.size()) return;
    auto& w = windows_[idx];
    if(!w.fullscreen)
    {
        w.savedX = w.x; w.savedY = w.y; w.savedW = w.w; w.savedH = w.h;
        w.x = 0; w.y = 0; w.w = resX_; w.h = resY_;
        w.fullscreen = true;
        w.minimized = false;
    }
    else
    {
        w.x = w.savedX; w.y = w.savedY; w.w = w.savedW; w.h = w.savedH;
        w.fullscreen = false;
    }
}

void WindowManager::applySnap(Window& w, SnapRegion region)
{
    w.minimized = false;
    w.fullscreen = false;
    switch(region)
    {
        case SnapRegion::Left:
            w.x = 0; w.y = 0; w.w = resX_ / 2; w.h = resY_; break;
        case SnapRegion::Right:
            w.x = resX_ / 2; w.y = 0; w.w = resX_ / 2; w.h = resY_; break;
        case SnapRegion::Top:
            w.x = 0; w.y = 0; w.w = resX_; w.h = resY_ / 2; break;
        case SnapRegion::Bottom:
            w.x = 0; w.y = resY_ / 2; w.w = resX_; w.h = resY_ / 2; break;
        case SnapRegion::TopLeft:
            w.x = 0; w.y = 0; w.w = resX_ / 2; w.h = resY_ / 2; break;
        case SnapRegion::TopRight:
            w.x = resX_ / 2; w.y = 0; w.w = resX_ / 2; w.h = resY_ / 2; break;
        case SnapRegion::BottomLeft:
            w.x = 0; w.y = resY_ / 2; w.w = resX_ / 2; w.h = resY_ / 2; break;
        case SnapRegion::BottomRight:
            w.x = resX_ / 2; w.y = resY_ / 2; w.w = resX_ / 2; w.h = resY_ / 2; break;
        case SnapRegion::LeftThird:
            w.x = 0; w.y = 0; w.w = resX_ / 3; w.h = resY_; break;
        case SnapRegion::CenterThird:
            w.w = resX_ / 3; w.h = resY_; w.x = (resX_ - w.w) / 2; w.y = 0; break;
        case SnapRegion::RightThird:
            w.w = resX_ / 3; w.h = resY_; w.x = resX_ - w.w; w.y = 0; break;
        case SnapRegion::Full:
            w.savedX = w.x; w.savedY = w.y; w.savedW = w.w; w.savedH = w.h;
            w.x = 0; w.y = 0; w.w = resX_; w.h = resY_; w.fullscreen = true; break;
        case SnapRegion::None:
        default:
            break;
    }
}

void WindowManager::snapFocused(SnapRegion region)
{
    if(focused_ < 0 || focused_ >= (int)windows_.size()) return;
    auto& w = windows_[focused_];
    applySnap(w, region);
}

void WindowManager::toggleMinimizeFocused()
{
    toggleMinimize(focused_);
}

void WindowManager::toggleFullscreenFocused()
{
    toggleFullscreen(focused_);
}

void WindowManager::handleMouse(int cx, int cy, bool left, bool right, bool prevLeft, bool prevRight)
{
    // Right click: desktop-only context menu
    if(right && !prevRight)
    {
        menu_.open = true;
        menu_.target = -1; // desktop
        menu_.x = cx;
        menu_.y = cy;
    }
    else if(!right && prevRight)
    {
        // closing menu on right release is optional; keep open until left click selection/outside
    }

    // Left click selection in context menu
    if(menu_.open && left && !prevLeft)
    {
        int items = 1; // desktop: new window
        int mx = cx - menu_.x;
        int my = cy - menu_.y;
        if(mx >= 0 && mx < menu_.width && my >= 0 && my < items * menu_.itemH)
        {
            int idx = my / menu_.itemH;
            if(idx == 0)
            {
                createDefaultWindow(menu_.x - 40, menu_.y - 20);
            }
            menu_.open = false;
            return; // don't start drag/resize on same click
        }
        else
        {
            menu_.open = false;
        }
    }
    else if(menu_.open && left && prevLeft)
    {
        // if menu is open and left is held, ignore drag/resize
        return;
    }

    // Left press: drag/resize
    if(left && !prevLeft)
    {
        activeWin_ = -1;
        dragging_ = false;
        resizing_ = false;
        snapRegion_ = SnapRegion::None;

        int picked = -1;
        bool pickedResize = false;
        bool pickedTitle = false;
        int offX = 0, offY = 0;
        int sW = 0, sH = 0;

        // pick topmost window under cursor
        for(int i = (int)windows_.size() - 1; i >= 0; --i)
        {
            auto& w = windows_[i];
            if(w.minimized) continue;
            bool inside = cx >= w.x && cx < w.x + w.w && cy >= w.y && cy < w.y + w.h;
            if(!inside) continue;
            bool inResize = (cx >= w.x + w.w - 10 && cy >= w.y + w.h - 10);
            bool inTitle = (cx >= w.x + 2 && cx <= w.x + w.w - 2 && cy >= w.y + 2 && cy <= w.y + 22);
            picked = i;
            pickedResize = inResize;
            pickedTitle = inTitle;
            offX = cx - w.x;
            offY = cy - w.y;
            sW = w.w; sH = w.h;
            break;
        }

        if(picked >= 0)
        {
            bringToFront(picked);
            activeWin_ = (int)windows_.size() - 1; // now at back
            focused_ = activeWin_;
            auto& w = windows_.back();
            if(pickedResize)
            {
                resizing_ = true;
                pressX_ = cx; pressY_ = cy; startW_ = sW; startH_ = sH;
            }
            else if(pickedTitle)
            {
                dragging_ = true;
                dragOffX_ = offX; dragOffY_ = offY;
                dragRestoreX_ = w.x; dragRestoreY_ = w.y; dragRestoreW_ = w.w; dragRestoreH_ = w.h;
                unSnapApplied_ = false;
            }
        }
    }
    else if(!left && prevLeft)
    {
        // On release, apply snap if dragging
        if(dragging_ && activeWin_ >= 0 && activeWin_ < (int)windows_.size())
        {
            auto& w = windows_[activeWin_];
            // Detect snap region (corners/quadrants, edges/halves, top-edge thirds)
            const int edge = 30;
            bool nearLeft = cx <= edge;
            bool nearRight = cx >= resX_ - edge;
            bool nearTop = cy <= edge;
            bool nearBottom = cy >= resY_ - edge;

            if(nearTop)
            {
                if(nearLeft) snapRegion_ = SnapRegion::TopLeft;
                else if(nearRight) snapRegion_ = SnapRegion::TopRight;
                else
                {
                    int third = resX_ / 3;
                    if(cx < third) snapRegion_ = SnapRegion::LeftThird;
                    else if(cx < 2 * third) snapRegion_ = SnapRegion::CenterThird;
                    else snapRegion_ = SnapRegion::RightThird;
                }
            }
            else if(nearBottom)
            {
                if(nearLeft) snapRegion_ = SnapRegion::BottomLeft;
                else if(nearRight) snapRegion_ = SnapRegion::BottomRight;
                else snapRegion_ = SnapRegion::Bottom;
            }
            else if(nearLeft)
            {
                snapRegion_ = SnapRegion::Left;
            }
            else if(nearRight)
            {
                snapRegion_ = SnapRegion::Right;
            }
            else
            {
                snapRegion_ = SnapRegion::None;
            }

            if(snapRegion_ != SnapRegion::None)
            {
                // Only save geometry when entering snap from a non-snap state
                if(lastSnapRegion_ == SnapRegion::None)
                {
                    w.savedX = w.x; w.savedY = w.y; w.savedW = w.w; w.savedH = w.h;
                }
                applySnap(w, snapRegion_);
                lastSnapRegion_ = snapRegion_;
            }
            else
            {
                // Release away from edges: stay where user dropped it, exit snap state
                lastSnapRegion_ = SnapRegion::None;
                // If dragged off-screen, pull back in and shrink if needed
                const int minW = 120, minH = 80;
                int dxLeft = std::max(0, -w.x);
                int dyTop = std::max(0, -w.y);
                int dxRight = std::max(0, w.x + w.w - resX_);
                int dyBottom = std::max(0, w.y + w.h - resY_);
                int overflow = dxLeft + dyTop + dxRight + dyBottom;

                // If overflow is significant, shrink width/height by 20% each time until fits
                if(overflow > 0)
                {
                    while(w.x < 0 || w.y < 0 || w.x + w.w > resX_ || w.y + w.h > resY_)
                    {
                        w.w = std::max(minW, (int)(w.w * 0.8));
                        w.h = std::max(minH, (int)(w.h * 0.8));
                        if(w.x + w.w > resX_) w.x = resX_ - w.w;
                        if(w.y + w.h > resY_) w.y = resY_ - w.h;
                        if(w.x < 0) w.x = 0;
                        if(w.y < 0) w.y = 0;
                    }
                }
                else
                {
                    // Just clamp without shrinking
                    if(w.x < 0) w.x = 0;
                    if(w.y < 0) w.y = 0;
                    if(w.x + w.w > resX_) w.x = resX_ - w.w;
                    if(w.y + w.h > resY_) w.y = resY_ - w.h;
                }
            }
        }

        dragging_ = false;
        resizing_ = false;
        activeWin_ = -1;
    }

    // Keep focused window topmost
    if(focused_ >= 0 && focused_ < (int)windows_.size() - 1)
    {
        auto w = windows_[focused_];
        windows_.erase(windows_.begin() + focused_);
        windows_.push_back(w);
        focused_ = (int)windows_.size() - 1;
        if(activeWin_ >= 0) activeWin_ = focused_;
    }

    // Apply drag/resize
    if(activeWin_ >= 0 && activeWin_ < (int)windows_.size())
    {
        auto& w = windows_[activeWin_];
        if(dragging_)
        {
            if(!unSnapApplied_)
            {
                // If we were snapped, restore saved pre-snap geometry once
                if(lastSnapRegion_ != SnapRegion::None)
                {
                    w.x = w.savedX; w.y = w.savedY; w.w = w.savedW; w.h = w.savedH;
                }
                else
                {
                    w.x = dragRestoreX_; w.y = dragRestoreY_; w.w = dragRestoreW_; w.h = dragRestoreH_;
                }
                w.fullscreen = false;
                w.minimized = false;
                unSnapApplied_ = true;
            }
            // Keep the original grab offset so the cursor stays anchored relative to the window
            w.x = cx - dragOffX_;
            w.y = cy - dragOffY_;
            if(w.x < 0) w.x = 0;
            if(w.y < 0) w.y = 0;
            if(w.x + w.w > resX_) w.x = resX_ - w.w;
            if(w.y + w.h > resY_) w.y = resY_ - w.h;
        }
        if(resizing_)
        {
            int newW = startW_ + (cx - pressX_);
            int newH = startH_ + (cy - pressY_);
            if(newW < 80) newW = 80;
            if(newH < 80) newH = 80;
            if(w.x + newW > resX_) newW = resX_ - w.x;
            if(w.y + newH > resY_) newH = resY_ - w.y;
            w.w = newW; w.h = newH;
        }
    }
}

void WindowManager::drawWindow(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h, const Window& win) const
{
    int rh = win.minimized ? 24 : win.h;
    if(rh < 24) rh = 24;
    // Border and title
    fillRect(fb, pitch, w, h, win.x, win.y, win.w, 2, win.border);
    fillRect(fb, pitch, w, h, win.x, win.y + rh - 2, win.w, 2, win.border);
    fillRect(fb, pitch, w, h, win.x, win.y, 2, rh, win.border);
    fillRect(fb, pitch, w, h, win.x + win.w - 2, win.y, 2, rh, win.border);
    // Title bar
    fillRect(fb, pitch, w, h, win.x + 2, win.y + 2, win.w - 4, 20, win.title);
    // Body if not minimized
    if(!win.minimized)
    {
        fillRect(fb, pitch, w, h, win.x + 2, win.y + 22, win.w - 4, rh - 24, win.bg);
    }
    // Resize handle (corner)
    fillRect(fb, pitch, w, h, win.x + win.w - 10, win.y + rh - 10, 10, 10, {180, 180, 180, 255});
}

void WindowManager::drawMenu(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h) const
{
    if(!menu_.open || menu_.target < 0 || menu_.target >= (int)windows_.size()) return;
    int items = 1;
    int mw = menu_.width;
    int mh = items * menu_.itemH;
    int mx = menu_.x;
    int my = menu_.y;
    // Clamp within screen
    if(mx + mw > w) mx = w - mw;
    if(my + mh > h) my = h - mh;
    // Background
    fillRect(fb, pitch, w, h, mx, my, mw, mh, {30, 30, 30, 255});
    // Desktop menu: New window
    fillRect(fb, pitch, w, h, mx, my, mw, menu_.itemH, {80, 160, 240, 255});
}

void WindowManager::draw(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h)
{
    for(const auto& win : windows_)
    {
        drawWindow(fb, pitch, w, h, win);
    }
    drawMenu(fb, pitch, w, h);
}
