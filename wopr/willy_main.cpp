// willy_main.cpp — standalone launcher for Willy the Worm, no WOPR terminal
// required. Wires the same wopr_willy_enter/update/render/keydown/mouse
// entry points that wopr.cpp normally drives, straight to an SDL2 window.
//
// Build (see accompanying notes for the miniz.h include-path caveat):
//   g++ -std=c++17 -O2 willy_main.cpp wopr_willy.cpp wopr_render.cpp
//       highscores.cpp -I. $(sdl2-config --cflags) -o willy $(sdl2-config --libs)
//
// The window is fixed at 1280x720. wopr_willy_render()/wopr_willy_update()
// fall back to exactly that size whenever SDL_GL_GetCurrentWindow() has no
// current GL context to report — which is always true here, since this
// build uses SDL_Renderer (via wopr_render.cpp) instead of raw OpenGL.
// Keeping the window at 1280x720 means that fallback is also the *correct*
// size, so layout still lines up. A resizable window would need an actual
// OpenGL context and a real gl_draw_* implementation instead.

#include "wopr.h"
#include "wopr_render.h"
#include <SDL2/SDL.h>
#include <cstdio>

static const int WINDOW_W = 1280;
static const int WINDOW_H = 720;

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Willy the Worm",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        // Fall back to software rendering if no accelerated driver is available.
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    gl_render_init(renderer);

    WoprState w;  // plain aggregate from wopr.h — only .lines/.sub_state are used here
    wopr_willy_enter(&w);
    if (!w.sub_state) {
        std::fprintf(stderr, "Willy failed to start:\n");
        for (auto &line : w.lines) std::fprintf(stderr, "%s\n", line.c_str());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Grid origin/cell metrics handed to wopr_willy_render()/_update(). cw/ch
    // must match what wopr_render actually draws a text cell as.
    const int px = 40, py = 40;
    const int cw = WOPR_MIN_FONT_CELL, ch = WOPR_MIN_FONT_CELL;
    const int cols = (WINDOW_W - px*2) / cw;

    bool running = true;
    Uint64 prev_ticks = SDL_GetPerformanceCounter();
    const Uint64 freq = SDL_GetPerformanceFrequency();

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (ev.key.keysym.sym == SDLK_F4 &&
                        (ev.key.keysym.mod & (KMOD_LALT | KMOD_RALT))) {
                        running = false;  // Alt+F4, since there's no WOPR shell to quit from
                        break;
                    }
                    wopr_willy_keydown(&w, ev.key.keysym.sym);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    wopr_willy_mousedown(&w, ev.button.x, ev.button.y, ev.button.button);
                    break;
                case SDL_MOUSEMOTION:
                    wopr_willy_mousemove(&w, ev.motion.x, ev.motion.y);
                    break;
                case SDL_MOUSEBUTTONUP:
                    wopr_willy_mouseup(&w, ev.button.x, ev.button.y, ev.button.button);
                    break;
                default:
                    break;
            }
        }

        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - prev_ticks) / (double)freq;
        prev_ticks = now;
        if (dt > 0.1) dt = 0.1;  // clamp huge stalls (window drag, breakpoint, etc.)

        wopr_willy_update(&w, dt);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        wopr_willy_render(&w, px, py, cw, ch, cols);
        SDL_RenderPresent(renderer);
    }

    wopr_willy_free(&w);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
