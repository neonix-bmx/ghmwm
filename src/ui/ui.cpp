#include "ui.hpp"
#include "text_renderer.hpp"

// Forward declaration (global drawText)
void drawText(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h,
              int x, int y, const std::string& text, Color fg);

namespace {
inline void fillRect(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h,
                     int x0, int y0, int rw, int rh, Color c)
{
    int x1 = x0 + rw; int y1 = y0 + rh;
    if(x0 < 0) x0 = 0; if(y0 < 0) y0 = 0;
    if(x1 > (int)w) x1 = (int)w; if(y1 > (int)h) y1 = (int)h;
    for(int y = y0; y < y1; ++y)
    {
        uint8_t* row = fb + y * pitch + x0 * 4;
        for(int x = x0; x < x1; ++x)
        {
            row[0] = c.b; row[1] = c.g; row[2] = c.r; row[3] = c.a;
            row += 4;
        }
    }
}
}

namespace ui
{
using ::drawText;
void drawCheckbox(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h, const Checkbox& cb)
{
    fillRect(fb, pitch, w, h, cb.x, cb.y, cb.size, cb.size, cb.bg);
    fillRect(fb, pitch, w, h, cb.x, cb.y, cb.size, 2, cb.border);
    fillRect(fb, pitch, w, h, cb.x, cb.y + cb.size - 2, cb.size, 2, cb.border);
    fillRect(fb, pitch, w, h, cb.x, cb.y, 2, cb.size, cb.border);
    fillRect(fb, pitch, w, h, cb.x + cb.size - 2, cb.y, 2, cb.size, cb.border);
    if(cb.checked)
    {
        // simple X mark
        for(int i = 2; i < cb.size - 2; ++i)
        {
            int px1 = cb.x + i;
            int py1 = cb.y + i;
            int px2 = cb.x + i;
            int py2 = cb.y + (cb.size - 1 - i);
            if(px1 >= 0 && px1 < (int)w && py1 >= 0 && py1 < (int)h)
            {
                uint8_t* p = fb + py1 * pitch + px1 * 4; p[0]=cb.fg.b; p[1]=cb.fg.g; p[2]=cb.fg.r; p[3]=cb.fg.a;
            }
            if(px2 >= 0 && px2 < (int)w && py2 >= 0 && py2 < (int)h)
            {
                uint8_t* p = fb + py2 * pitch + px2 * 4; p[0]=cb.fg.b; p[1]=cb.fg.g; p[2]=cb.fg.r; p[3]=cb.fg.a;
            }
        }
    }
}

void drawScrollbar(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h, const Scrollbar& sb)
{
    if(sb.vertical)
    {
        fillRect(fb, pitch, w, h, sb.x, sb.y, sb.thickness, sb.length, sb.track);
        int knobLen = (int)(sb.length * sb.knobSize);
        if(knobLen < 8) knobLen = 8;
        int knobPos = sb.y + (int)((sb.length - knobLen) * sb.value);
        fillRect(fb, pitch, w, h, sb.x, knobPos, sb.thickness, knobLen, sb.knob);
        fillRect(fb, pitch, w, h, sb.x, sb.y, sb.thickness, 2, sb.border);
        fillRect(fb, pitch, w, h, sb.x, sb.y + sb.length - 2, sb.thickness, 2, sb.border);
    }
    else
    {
        fillRect(fb, pitch, w, h, sb.x, sb.y, sb.length, sb.thickness, sb.track);
        int knobLen = (int)(sb.length * sb.knobSize);
        if(knobLen < 8) knobLen = 8;
        int knobPos = sb.x + (int)((sb.length - knobLen) * sb.value);
        fillRect(fb, pitch, w, h, knobPos, sb.y, knobLen, sb.thickness, sb.knob);
        fillRect(fb, pitch, w, h, sb.x, sb.y, 2, sb.thickness, sb.border);
        fillRect(fb, pitch, w, h, sb.x + sb.length - 2, sb.y, 2, sb.thickness, sb.border);
    }
}

void drawTextInput(uint8_t* fb, uint32_t pitch, uint32_t sw, uint32_t sh, const TextInput& ti)
{
    fillRect(fb, pitch, sw, sh, ti.x, ti.y, ti.w, ti.h, ti.bg);
    fillRect(fb, pitch, sw, sh, ti.x, ti.y, ti.w, 2, ti.border);
    fillRect(fb, pitch, sw, sh, ti.x, ti.y + ti.h - 2, ti.w, 2, ti.border);
    fillRect(fb, pitch, sw, sh, ti.x, ti.y, 2, ti.h, ti.border);
    fillRect(fb, pitch, sw, sh, ti.x + ti.w - 2, ti.y, 2, ti.h, ti.border);
    int textX = ti.x + 4;
    int textY = ti.y + (ti.h - 8) / 2;
    ::drawText(fb, pitch, sw, sh, textX, textY, ti.text, ti.fg);
    if(ti.focused)
    {
        int cursorPx = textX + (int)ti.cursor * 8;
        fillRect(fb, pitch, sw, sh, cursorPx, textY, 2, 8, ti.border);
    }
}

void handleCheckboxClick(Checkbox& cb, int mx, int my, bool pressed, bool prevPressed)
{
    if(pressed && !prevPressed)
    {
        bool inside = mx >= cb.x && mx < cb.x + cb.size && my >= cb.y && my < cb.y + cb.size;
        if(inside) cb.checked = !cb.checked;
    }
}

void handleScrollbarDrag(Scrollbar& sb, int mx, int my, bool pressed, bool prevPressed)
{
    auto clamp01 = [](float v){ return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
    if(pressed && !prevPressed)
    {
        // start drag if inside knob
        if(sb.vertical)
        {
            int knobLen = (int)(sb.length * sb.knobSize); if(knobLen < 8) knobLen = 8;
            int knobPos = sb.y + (int)((sb.length - knobLen) * sb.value);
            if(mx >= sb.x && mx < sb.x + sb.thickness && my >= knobPos && my < knobPos + knobLen)
            {
                sb.dragging = true;
                sb.dragStartPos = my;
                sb.dragStartValue = sb.value;
            }
        }
        else
        {
            int knobLen = (int)(sb.length * sb.knobSize); if(knobLen < 8) knobLen = 8;
            int knobPos = sb.x + (int)((sb.length - knobLen) * sb.value);
            if(my >= sb.y && my < sb.y + sb.thickness && mx >= knobPos && mx < knobPos + knobLen)
            {
                sb.dragging = true;
                sb.dragStartPos = mx;
                sb.dragStartValue = sb.value;
            }
        }
    }
    else if(!pressed && prevPressed)
    {
        sb.dragging = false;
    }

    if(sb.dragging && pressed)
    {
        if(sb.vertical)
        {
            int delta = my - sb.dragStartPos;
            int knobLen = (int)(sb.length * sb.knobSize); if(knobLen < 8) knobLen = 8;
            int travel = sb.length - knobLen;
            if(travel <= 0) sb.value = 0.0f;
            else sb.value = clamp01(sb.dragStartValue + (float)delta / (float)travel);
        }
        else
        {
            int delta = mx - sb.dragStartPos;
            int knobLen = (int)(sb.length * sb.knobSize); if(knobLen < 8) knobLen = 8;
            int travel = sb.length - knobLen;
            if(travel <= 0) sb.value = 0.0f;
            else sb.value = clamp01(sb.dragStartValue + (float)delta / (float)travel);
        }
    }
}

void handleTextInputClick(TextInput& ti, int mx, int my, bool pressed, bool prevPressed)
{
    if(pressed && !prevPressed)
    {
        bool inside = mx >= ti.x && mx < ti.x + ti.w && my >= ti.y && my < ti.y + ti.h;
        ti.focused = inside;
        if(inside)
        {
            // crude cursor positioning: proportional to x offset
            int rel = mx - (ti.x + 4);
            int pos = rel / 8;
            if(pos < 0) pos = 0;
            if(pos > (int)ti.text.size()) pos = (int)ti.text.size();
            ti.cursor = (size_t)pos;
        }
    }
}

void handleTextInputKey(TextInput& ti, const std::string& key, bool ctrl, bool alt, bool shift)
{
    if(!ti.focused) return;
    if(ctrl || alt) return; // ignore combos for now
    if(key == "KEY_BACKSPACE")
    {
        if(ti.cursor > 0 && !ti.text.empty())
        {
            ti.text.erase(ti.cursor - 1, 1);
            --ti.cursor;
        }
        return;
    }
    if(key == "KEY_DELETE")
    {
        if(ti.cursor < ti.text.size())
        {
            ti.text.erase(ti.cursor, 1);
        }
        return;
    }
    if(key == "KEY_LEFT")
    {
        if(ti.cursor > 0) --ti.cursor;
        return;
    }
    if(key == "KEY_RIGHT")
    {
        if(ti.cursor < ti.text.size()) ++ti.cursor;
        return;
    }
    if(key.size() == 1)
    {
        char ch = key[0];
        if(ch >= 32 && ch <= 126)
        {
            ti.text.insert(ti.text.begin() + (int)ti.cursor, ch);
            ++ti.cursor;
        }
    }
}

} // namespace ui
