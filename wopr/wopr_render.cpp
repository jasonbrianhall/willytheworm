#include "wopr_render.h"
#include "font8x8_basic.h"
#include <cstring>

static SDL_Renderer *g_renderer = nullptr;

// Each font8x8_basic bit is drawn as a FONT_PIXEL x FONT_PIXEL screen
// rectangle, so one 8x8 glyph is 8*FONT_PIXEL screen pixels square.
static const float FONT_PIXEL = 2.0f;

const int WOPR_MIN_FONT_CELL = (int)(8 * FONT_PIXEL);

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
    for (const unsigned char *p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
        unsigned char c = (*p < 128) ? *p : '?';
        const unsigned char *glyph = font8x8_basic[c];
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
    return (float)strlen(text) * 8.f * FONT_PIXEL * scale;
}

float gl_text_height(float scale) {
    return 8.f * FONT_PIXEL * scale;
}

void gl_flush_verts() {
    // SDL_Renderer draws happen immediately, so there's nothing to batch.
    // Present the frame yourself once per loop iteration (see willy_main.cpp).
}
