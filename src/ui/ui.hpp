#pragma once

#include <string>
#include <cstdint>
#include "window_manager.hpp"
#include "text_renderer.hpp"

namespace ui
{
struct Checkbox
{
    int x = 0, y = 0, size = 16;
    bool checked = false;
    Color fg{0,0,0,255};
    Color bg{230,230,230,255};
    Color border{80,80,80,255};
};

struct Scrollbar
{
    int x = 0, y = 0;
    int length = 100;      // along the scroll direction
    int thickness = 10;
    bool vertical = true;
    float value = 0.0f;    // 0..1
    float knobSize = 0.2f; // fraction of length
    bool dragging = false;
    int dragStartPos = 0;
    float dragStartValue = 0.0f;
    Color track{230,230,230,255};
    Color knob{120,120,120,255};
    Color border{60,60,60,255};
};

struct TextInput
{
    int x = 0, y = 0, w = 150, h = 20;
    std::string text;
    size_t cursor = 0;
    bool focused = false;
    Color bg{255,255,255,255};
    Color border{80,80,80,255};
    Color fg{0,0,0,255};
};

// Drawing
void drawCheckbox(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h, const Checkbox& cb);
void drawScrollbar(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h, const Scrollbar& sb);
void drawTextInput(uint8_t* fb, uint32_t pitch, uint32_t sw, uint32_t sh, const TextInput& ti);

// Interaction helpers
void handleCheckboxClick(Checkbox& cb, int mx, int my, bool pressed, bool prevPressed);
void handleScrollbarDrag(Scrollbar& sb, int mx, int my, bool pressed, bool prevPressed);
void handleTextInputKey(TextInput& ti, const std::string& key, bool ctrl, bool alt, bool shift);
void handleTextInputClick(TextInput& ti, int mx, int my, bool pressed, bool prevPressed);
}
