#include "wopr_render.h"
#include "font8x8_basic.h"
#include <cstdint>
#include <cstring>

static SDL_Renderer *g_renderer = nullptr;

// Each font8x8_basic bit is drawn as a FONT_PIXEL x FONT_PIXEL screen
// rectangle, so one 8x8 glyph is 8*FONT_PIXEL screen pixels square.
static const float FONT_PIXEL = 2.0f;

const int WOPR_MIN_FONT_CELL = (int)(8 * FONT_PIXEL);

// font8x8_basic only covers ASCII (U+0000-U+007F). The WOPR intro screen's
// "use the arrow keys" line uses the actual Unicode arrows (U+2190 LEFT,
// U+2191 UP, U+2192 RIGHT, U+2193 DOWN), so those four are hand-drawn here
// rather than falling back to '?'. Same bit convention as font8x8_basic
// (bit 0 = leftmost pixel in the row).
static const unsigned char GLYPH_ARROW_LEFT[8]  = {0x00,0x10,0x18,0x3E,0x18,0x10,0x00,0x00};
static const unsigned char GLYPH_ARROW_UP[8]    = {0x08,0x1C,0x3E,0x08,0x08,0x08,0x08,0x00};
static const unsigned char GLYPH_ARROW_RIGHT[8] = {0x00,0x08,0x18,0x7C,0x18,0x08,0x00,0x00};
static const unsigned char GLYPH_ARROW_DOWN[8]  = {0x08,0x08,0x08,0x08,0x3E,0x1C,0x08,0x00};

// Decodes one UTF-8 codepoint starting at p, returns the codepoint and
// advances *bytes_consumed accordingly. Only handles 1-3 byte sequences
// (everything this codebase's text actually uses); malformed or 4-byte
// sequences fall back to a single replacement byte so we never get stuck.
static uint32_t utf8_decode(const unsigned char *p, int *bytes_consumed) {
    if (p[0] < 0x80) { *bytes_consumed = 1; return p[0]; }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *bytes_consumed = 2;
        return ((uint32_t)(p[0] & 0x1F) << 6) | (uint32_t)(p[1] & 0x3F);
    }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *bytes_consumed = 3;
        return ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (uint32_t)(p[2] & 0x3F);
    }
    *bytes_consumed = 1;
    return 0xFFFD;  // replacement character — draws as '?' below
}

static const unsigned char *glyph_for(uint32_t cp) {
    switch (cp) {
        case 0x2190: return GLYPH_ARROW_LEFT;
        case 0x2191: return GLYPH_ARROW_UP;
        case 0x2192: return GLYPH_ARROW_RIGHT;
        case 0x2193: return GLYPH_ARROW_DOWN;
        default: break;
    }
    if (cp < 128) return font8x8_basic[cp];
    return font8x8_basic['?'];
}

// Number of glyph cells text will draw as (not byte length — multi-byte
// UTF-8 sequences are one cell), used by both gl_draw_text and gl_text_width
// so they agree on layout.
static size_t glyph_count(const char *text) {
    size_t n = 0;
    const unsigned char *p = reinterpret_cast<const unsigned char*>(text);
    while (*p) {
        int consumed;
        utf8_decode(p, &consumed);
        p += consumed;
        n++;
    }
    return n;
}

void gl_render_init(SDL_Renderer *renderer) {
    g_renderer = renderer;
    if (g_renderer) SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
}

void gl_draw_rect(float x, float y, float w, float h,
                   float r, float g, float b, float a) {
    if (!g_renderer) return;
    SDL_SetRenderDrawColor(g_renderer,
        (Uint8)(r * 255.f), (Uint8)(g * 255.f), (Uint8)(b * 255.f), (Uint8)(a * 255.f));
    SDL_FRect rect{ x, y, w, h };
    SDL_RenderFillRectF(g_renderer, &rect);
}

void gl_draw_text(const char *text, float x, float y,
                   float r, float g, float b, float a, float scale) {
    if (!g_renderer || !text) return;
    SDL_SetRenderDrawColor(g_renderer,
        (Uint8)(r * 255.f), (Uint8)(g * 255.f), (Uint8)(b * 255.f), (Uint8)(a * 255.f));

    float px = FONT_PIXEL * scale;
    float cx = x;
    const unsigned char *p = reinterpret_cast<const unsigned char*>(text);
    while (*p) {
        int consumed;
        uint32_t cp = utf8_decode(p, &consumed);
        p += consumed;

        const unsigned char *glyph = glyph_for(cp);
        for (int row = 0; row < 8; row++) {
            unsigned char bits = glyph[row];
            if (!bits) continue;
            for (int col = 0; col < 8; col++) {
                if (bits & (1 << col)) {
                    SDL_FRect r2{ cx + col * px, y + row * px, px, px };
                    SDL_RenderFillRectF(g_renderer, &r2);
                }
            }
        }
        cx += 8 * px;
    }
}

float gl_text_width(const char *text, float scale) {
    if (!text) return 0.f;
    return (float)glyph_count(text) * 8.f * FONT_PIXEL * scale;
}

float gl_text_height(float scale) {
    return 8.f * FONT_PIXEL * scale;
}

void gl_flush_verts() {
    // SDL_Renderer draws happen immediately, so there's nothing to batch.
    // Present the frame yourself once per loop iteration (see willy_main.cpp).
}
