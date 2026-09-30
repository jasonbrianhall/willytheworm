#pragma once
#include <stdint.h>

// The game draws into this back buffer (0x00RRGGBB); kernel.cpp copies it to
// the framebuffer once per frame.
extern uint32_t* back;
extern uint32_t back_w, back_h;

void render_set_screen(uint32_t w, uint32_t h);   // render.cpp: picks the font
