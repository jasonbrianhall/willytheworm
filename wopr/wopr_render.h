// wopr_render.h — standalone stand-in for the WOPR terminal's renderer.
//
// wopr_willy.cpp only ever calls five drawing functions:
//   gl_draw_rect(), gl_draw_text(), gl_text_width(), gl_text_height(),
//   gl_flush_verts()
// This header/impl provides exactly those five, implemented with plain
// SDL2 (SDL_Renderer) and the embedded font8x8_basic bitmap font — no
// OpenGL, no WOPR terminal, no other WOPR source files required.
//
// Not a drop-in replacement for the real wopr_render.h used by the full
// WOPR build (it doesn't batch vertices, doesn't do GL, etc.) — it exists
// only to let wopr_willy.cpp link and run on its own.
#pragma once

#include <SDL2/SDL.h>

// Must be called once, after creating the SDL_Renderer, before any
// gl_draw_* call.
void gl_render_init(SDL_Renderer *renderer);

// Pixel width/height of one monospace text cell at scale=1.0. Pass this
// as the cw/ch arguments to wopr_willy_render()/wopr_willy_update() so
// its internal "cs = (float)cw" text-cell math lines up with what this
// renderer actually draws.
extern const int WOPR_MIN_FONT_CELL;

void  gl_draw_rect(float x, float y, float w, float h,
                    float r, float g, float b, float a);
void  gl_draw_text(const char *text, float x, float y,
                    float r, float g, float b, float a, float scale);
float gl_text_width(const char *text, float scale);
float gl_text_height(float scale);
void  gl_flush_verts();  // no-op here — SDL_Renderer draws are immediate
