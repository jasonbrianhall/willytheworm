// Bare-metal replacement for wopr_render.cpp: the five drawing calls
// wopr_willy.cpp uses, rendered in software into the kernel's back buffer.
// Text uses the desktop build's DejaVu Sans Mono at the same 20 px size,
// pre-rasterized with anti-aliasing (dejavu20.h, 12x24 cells) and scaled by
// an integer factor on big screens. Screens narrower than 1280 px get the
// 8x16 VGA console font (font.h) instead so the status line still fits.
#include <stdint.h>
#include "wopr_render.h"
#include "video.hpp"
#include "font.h"
#include "dejavu20.h"

static int g_fs = 1;                       // font scale
static bool g_vga;                         // small screen: 8x16 VGA font
static inline int cell_w() { return (g_vga ? 8 : FONT_CELL_W) * g_fs; }
static inline int cell_h() { return (g_vga ? 16 : FONT_CELL_H) * g_fs; }

void render_set_screen(uint32_t w, uint32_t h) {
    g_vga = w < 1280;
    int s = g_vga ? 1 : (int)(w / 1280 < h / 720 ? w / 1280 : h / 720);
    g_fs = s < 1 ? 1 : s;
}
int gl_font_cell_size() { return cell_w(); }
bool gl_render_init(SDL_Renderer*) { return true; }
void gl_render_shutdown() {}
void gl_flush_verts() {}

static inline int iround(float v) { return (int)(v < 0 ? v - 0.5f : v + 0.5f); }
static inline uint32_t chan(float c) { return c <= 0.f ? 0 : c >= 1.f ? 255 : (uint32_t)(c * 255.f + 0.5f); }

static void fill_px(int x0, int y0, int x1, int y1, uint32_t rgb, uint32_t a) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)back_w) x1 = back_w;
    if (y1 > (int)back_h) y1 = back_h;
    if (x0 >= x1 || y0 >= y1 || a == 0) return;
    if (a >= 255) {
        for (int y = y0; y < y1; y++) {
            uint32_t* row = back + (size_t)y * back_w;
            for (int x = x0; x < x1; x++) row[x] = rgb;
        }
        return;
    }
    uint32_t sr = (rgb >> 16) & 255, sg = (rgb >> 8) & 255, sb = rgb & 255, ia = 255 - a;
    for (int y = y0; y < y1; y++) {
        uint32_t* row = back + (size_t)y * back_w;
        for (int x = x0; x < x1; x++) {
            uint32_t d = row[x];
            uint32_t r = (sr * a + ((d >> 16) & 255) * ia) / 255;
            uint32_t g = (sg * a + ((d >> 8) & 255) * ia) / 255;
            uint32_t b = (sb * a + (d & 255) * ia) / 255;
            row[x] = r << 16 | g << 8 | b;
        }
    }
}

void gl_draw_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    // Round both edges so adjacent rects (sprite pixels) tile without gaps.
    fill_px(iround(x), iround(y), iround(x + w), iround(y + h),
            chan(r) << 16 | chan(g) << 8 | chan(b), chan(a));
}

// Arrow glyphs (U+2190..U+2193), 8x8 with bit 0 = leftmost pixel, drawn at
// double height to match the 8x16 font.
static const uint8_t ARROWS[4][8] = {
    {0x00,0x10,0x18,0x3E,0x18,0x10,0x00,0x00},   // left (same bitmaps as wopr_render.cpp)
    {0x08,0x1C,0x3E,0x08,0x08,0x08,0x08,0x00},   // up
    {0x00,0x08,0x18,0x7C,0x18,0x08,0x00,0x00},   // right
    {0x08,0x08,0x08,0x08,0x3E,0x1C,0x08,0x00},   // down
};
static uint8_t arrow_row(int which, int row) { return ARROWS[which][row]; }

static uint32_t utf8_next(const unsigned char*& p) {
    uint32_t c = *p;
    if (c < 0x80) { p++; return c; }
    if ((c & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) { c = (c & 0x1F) << 6 | (p[1] & 0x3F); p += 2; return c; }
    if ((c & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        c = (c & 0x0F) << 12 | (p[1] & 0x3F) << 6 | (p[2] & 0x3F); p += 3; return c;
    }
    p++;
    return '?';
}

void gl_draw_text(const char* text, float x, float y, float r, float g, float b, float a, float scale) {
    if (!text) return;
    int s = iround((float)g_fs * (scale > 0.f ? scale : 1.f));
    if (s < 1) s = 1;
    uint32_t rgb = chan(r) << 16 | chan(g) << 8 | chan(b), al = chan(a);
    int cx = iround(x), cy = iround(y);
    const unsigned char* p = (const unsigned char*)text;
    while (*p) {
        uint32_t cp = utf8_next(p);
        bool arrow = cp >= 0x2190 && cp <= 0x2193;
        if (!arrow && (cp < 32 || cp > 126)) cp = '?';
        if (g_vga) {
            for (int row = 0; row < 16; row++) {
                uint8_t bits = arrow ? arrow_row(cp - 0x2190, row / 2) : font8x16[cp - 32][row];
                for (int col = 0; col < 8; col++) {
                    bool on = arrow ? (bits >> col) & 1 : (bits >> (7 - col)) & 1;
                    if (on) fill_px(cx + col * s, cy + row * s, cx + (col + 1) * s, cy + (row + 1) * s, rgb, al);
                }
            }
            cx += 8 * s;
        } else {
            const uint8_t (*gl)[FONT_CELL_W] = font_cov[arrow ? 95 + (cp - 0x2190) : cp - 32];
            for (int row = 0; row < FONT_CELL_H; row++)
                for (int col = 0; col < FONT_CELL_W; col++)
                    if (uint32_t c = gl[row][col])
                        fill_px(cx + col * s, cy + row * s, cx + (col + 1) * s, cy + (row + 1) * s, rgb, c * al / 255);
            cx += FONT_CELL_W * s;
        }
    }
}

float gl_text_width(const char* text, float scale) {
    int n = 0;
    for (const unsigned char* p = (const unsigned char*)text; p && *p; n++) utf8_next(p);
    return (float)n * cell_w() * scale;
}
float gl_text_height(float scale) { return (float)cell_h() * scale; }
