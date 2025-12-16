#include <ghost.h>
#include <libvideo/videodriver.hpp>
#include <libps2driver/ps2driver.hpp>
#include <libinput/mouse/mouse.hpp>
#include <libinput/keyboard/keyboard.hpp>
#include <png.h>

#include <cstdint>
#include <cstdio>
#include <atomic>
#include <vector>
#include <string>
#include <cstring>

#include "window_manager.hpp"

namespace
{

std::atomic<int> gCursorX{0};
std::atomic<int> gCursorY{0};
std::atomic<int> gResX{1024};
std::atomic<int> gResY{768};
std::atomic<int> gBtn1{0};
std::atomic<int> gBtn2{0};
static g_fd gMouseFd = 0;
static g_fd gKbFd = 0;

// Global WM instance
static WindowManager gWm;

bool waitForVideoDevice(g_tid& outDriver, g_device_id& outDevice)
{
    auto tx = G_MESSAGE_TOPIC_TRANSACTION_START;
    uint8_t buf[1024];

    while(true)
    {
        auto status = g_receive_topic_message(G_DEVICE_EVENT_TOPIC, buf, sizeof(buf), tx);
        if(status != G_MESSAGE_RECEIVE_STATUS_SUCCESSFUL)
            continue;

        auto header = reinterpret_cast<g_message_header*>(buf);
        tx = header->transaction;
        auto content = reinterpret_cast<g_device_event_header*>(G_MESSAGE_CONTENT(header));

        if(content->event == G_DEVICE_EVENT_DEVICE_REGISTERED)
        {
            auto ev = reinterpret_cast<g_device_event_device_registered*>(content);
            if(ev->type == G_DEVICE_TYPE_VIDEO)
            {
                outDriver = ev->driver;
                outDevice = ev->id;
                return true;
            }
        }
    }
}

// Minimal RGBA helpers

void fillRect(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h,
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


struct CursorImage
{
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba; // width * height * 4
};

static bool loadPngRGBA(const char* path, CursorImage& out)
{
    FILE* fp = fopen(path, "rb");
    if(!fp) return false;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if(!png) { fclose(fp); return false; }
    png_infop info = png_create_info_struct(png);
    if(!info) { png_destroy_read_struct(&png, nullptr, nullptr); fclose(fp); return false; }
    if(setjmp(png_jmpbuf(png))) { png_destroy_read_struct(&png, &info, nullptr); fclose(fp); return false; }

    png_init_io(png, fp);
    png_read_info(png, info);

    png_uint_32 width = png_get_image_width(png, info);
    png_uint_32 height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if(bit_depth == 16) png_set_strip_16(png);
    if(color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if(!(color_type & PNG_COLOR_MASK_ALPHA)) png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
    if(color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    out.width = width;
    out.height = height;
    out.rgba.resize(width * height * 4);

    std::vector<png_bytep> rows(height);
    for(png_uint_32 y = 0; y < height; ++y)
        rows[y] = (png_bytep) (&out.rgba[y * width * 4]);

    png_read_image(png, rows.data());
    png_destroy_read_struct(&png, &info, nullptr);
    fclose(fp);
    return true;
}

void blitCursor(const CursorImage& cur, uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h, int cx, int cy)
{
    for(uint32_t y = 0; y < cur.height; ++y)
    {
        int dy = cy + (int)y;
        if(dy < 0 || dy >= (int)h) continue;
        const uint8_t* srcRow = &cur.rgba[y * cur.width * 4];
        uint8_t* dstRow = fb + dy * pitch;
        for(uint32_t x = 0; x < cur.width; ++x)
        {
            int dx = cx + (int)x;
            if(dx < 0 || dx >= (int)w) continue;
            const uint8_t* s = srcRow + x * 4;
            uint8_t* d = dstRow + dx * 4;
            uint8_t sa = s[3];
            if(sa == 0) continue;
            if(sa == 255)
            {
                d[0] = s[2];
                d[1] = s[1];
                d[2] = s[0];
                d[3] = 0xFF;
            }
            else
            {
                for(int c = 0; c < 3; ++c)
                {
                    uint8_t sc = s[2 - c];
                    uint8_t dc = d[c];
                    d[c] = (uint8_t)((sc * sa + dc * (255 - sa)) / 255);
                }
                d[3] = 0xFF;
            }
        }
    }
}

void drawScene(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h, uint32_t frame)
{
    // Background gradient
    for(uint32_t y = 0; y < h; ++y)
    {
        uint8_t* row = fb + y * pitch;
        uint8_t g = (uint8_t)((y * 180) / h);
        for(uint32_t x = 0; x < w; ++x)
        {
            uint8_t r = (uint8_t)((x * 80) / w + 30);
            row[0] = 0; row[1] = g; row[2] = r; row[3] = 0xFF;
            row += 4;
        }
    }

    // (Animation removed to reduce flicker)
}

void drawCursor(uint8_t* fb, uint32_t pitch, uint32_t w, uint32_t h, int cursorX, int cursorY)
{
    static CursorImage cursorImg;
    static bool cursorLoaded = false;
    if(!cursorLoaded)
    {
        cursorLoaded = loadPngRGBA("/system/graphics/cursor/default.cursor/default.png", cursorImg) ||
                       loadPngRGBA("/system/graphics/cursor/default.png", cursorImg);
    }
    if(cursorLoaded)
    {
        blitCursor(cursorImg, fb, pitch, w, h, cursorX, cursorY);
    }
    else
    {
        // fallback crosshair
        static const uint32_t cursorSize = 10;
        static const Color cursorCol{255, 255, 255, 255};
        static const Color cursorShadow{0, 0, 0, 255};
        for(int dy = -1; dy <= 1; ++dy)
        {
            for(int dx = -1; dx <= 1; ++dx)
            {
                int px = cursorX + dx; int py = cursorY + dy;
                if(px >= 0 && py >= 0 && px < (int)w && py < (int)h)
                {
                    uint8_t* p = fb + py * pitch + px * 4;
                    p[0] = cursorShadow.b; p[1] = cursorShadow.g; p[2] = cursorShadow.r; p[3] = cursorShadow.a;
                }
            }
        }
        for(int i = -cursorSize; i <= cursorSize; ++i)
        {
            int px = cursorX + i;
            if(px >= 0 && px < (int)w && cursorY >= 0 && cursorY < (int)h)
            {
                uint8_t* p = fb + cursorY * pitch + px * 4;
                p[0] = cursorCol.b; p[1] = cursorCol.g; p[2] = cursorCol.r; p[3] = cursorCol.a;
            }
            int py = cursorY + i;
            if(py >= 0 && py < (int)h && cursorX >= 0 && cursorX < (int)w)
            {
                uint8_t* p = fb + py * pitch + cursorX * 4;
                p[0] = cursorCol.b; p[1] = cursorCol.g; p[2] = cursorCol.r; p[3] = cursorCol.a;
            }
        }
    }
}

} // namespace

int main()
{
    if(!g_task_register_name("ghmwm"))
    {
        printf("ghmwm: failed to register task name\n");
        return -1;
    }

    printf("ghmwm: waiting for video device...\n");
    g_tid driverTid = 0;
    g_device_id deviceId = 0;
    if(!waitForVideoDevice(driverTid, deviceId))
    {
        printf("ghmwm: no video device found\n");
        return -1;
    }
    printf("ghmwm: got video device id=%u driver=%i\n", deviceId, driverTid);

    g_video_mode_info mode{};
    uint16_t reqW = 1024;
    uint16_t reqH = 768;
    uint8_t reqBpp = 32;
    if(!videoDriverSetMode(driverTid, deviceId, reqW, reqH, reqBpp, mode))
    {
        printf("ghmwm: videoDriverSetMode %ux%u@%u failed\n", reqW, reqH, reqBpp);
        return -1;
    }
    printf("ghmwm: mode set res=%ux%u bpp=%u bpsl=%u lfb=%p explicit_update=%i\n",
         mode.resX, mode.resY, mode.bpp, mode.bpsl, (void*)mode.lfb, mode.explicit_update);

    // Mouse setup
    g_fd kbFd;
    if(!ps2DriverInitialize(&kbFd, &gMouseFd))
    {
        printf("ghmwm: failed to init ps2driver\n");
        return -1;
    }
    gKbFd = kbFd;

    gResX.store(mode.resX);
    gResY.store(mode.resY);
    gCursorX.store(mode.resX / 2);
    gCursorY.store(mode.resY / 2);

    gWm.init(mode.resX, mode.resY);

    // Spawn mouse reader task (no args)
    auto mouseReader = []()
    {
        g_mouse_info info{};
        while(true)
        {
            info = g_mouse::readMouse(gMouseFd);
            int x = gCursorX.load();
            int y = gCursorY.load();
            x += info.x;
            y += info.y; // PS/2 packet: positive y is down, so add
            int maxX = gResX.load();
            int maxY = gResY.load();
            if(x < 0) x = 0; if(y < 0) y = 0;
            if(x >= maxX) x = maxX - 1;
            if(y >= maxY) y = maxY - 1;
            gCursorX.store(x);
            gCursorY.store(y);
            gBtn1.store(info.button1 ? 1 : 0);
            gBtn2.store(info.button2 ? 1 : 0);
        }
    };
    g_create_task((void*) +mouseReader);

    // Spawn keyboard reader task (no args)
    auto keyboardReader = []()
    {
        while(true)
        {
            g_key_info key = g_keyboard::readKey(gKbFd);
            if(!key.pressed) continue;
            // Snap shortcuts: Ctrl+Alt+Arrow
            if(key.ctrl && key.alt)
            {
                if(key.key == "KEY_LEFT") gWm.snapFocused(WindowManager::SnapRegion::Left);
                else if(key.key == "KEY_RIGHT") gWm.snapFocused(WindowManager::SnapRegion::Right);
                else if(key.key == "KEY_UP") gWm.snapFocused(WindowManager::SnapRegion::Top);
                else if(key.key == "KEY_DOWN") gWm.snapFocused(WindowManager::SnapRegion::Bottom);
            }
            // Fullscreen: F11
            if(key.key == "KEY_F11") gWm.toggleFullscreenFocused();
            // Minimize: Ctrl+M
            if(key.ctrl && key.key == "KEY_M") gWm.toggleMinimizeFocused();
        }
    };
    g_create_task((void*) +keyboardReader);

    uint8_t* fb = reinterpret_cast<uint8_t*>(mode.lfb);
    uint32_t pitch = mode.bpsl;

    // Offscreen back buffer to reduce tearing/flicker
    std::vector<uint8_t> backbuf(pitch * mode.resY);
    uint8_t* bb = backbuf.data();

    uint32_t frame = 0;
    int prevBtn = 0;
    int prevBtnR = 0;

    while(true)
    {
        // Clamp cursor each frame to current resolution
        int cx = gCursorX.load();
        int cy = gCursorY.load();
        if(cx < 0) cx = 0; if(cy < 0) cy = 0;
        if(cx >= (int)mode.resX) cx = mode.resX - 1;
        if(cy >= (int)mode.resY) cy = mode.resY - 1;
        gCursorX.store(cx); gCursorY.store(cy);

        int btnL = gBtn1.load();
        int btnR = gBtn2.load();

        gWm.handleMouse(cx, cy, btnL, btnR, prevBtn, prevBtnR);
        prevBtn = btnL;
        prevBtnR = btnR;

        drawScene(bb, pitch, mode.resX, mode.resY, frame++);
        gWm.draw(bb, pitch, mode.resX, mode.resY);
        drawCursor(bb, pitch, mode.resX, mode.resY, cx, cy);

        std::memcpy(fb, bb, backbuf.size());

        if(mode.explicit_update)
        {
            videoDriverUpdate(driverTid, deviceId, 0, 0, mode.resX, mode.resY);
        }

        g_sleep(16);
    }

    return 0;
}
