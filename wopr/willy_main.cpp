// willy_main.cpp — standalone launcher for Willy the Worm, no WOPR terminal
// required. Wires the same wopr_willy_enter/update/render/keydown/mouse
// entry points that wopr.cpp normally drives, straight to an SDL2 window.
//
// Build (see accompanying notes for the miniz.h include-path caveat):
//   g++ -std=c++17 -O2 willy_main.cpp wopr_willy.cpp wopr_render.cpp
//       highscores.cpp -I. $(sdl2-config --cflags) -o willy $(sdl2-config --libs)
//
// The window opens at 1280x720 but is resizable, maximizable, and F11
// toggles fullscreen. wopr_willy_render() sizes itself off whatever
// SDL_GL_GetCurrentWindow() reports, which only works with a real GL
// context current — see the SDL_HINT_RENDER_DRIVER note below for how
// that's guaranteed here even though drawing itself goes through
// SDL_Renderer, not raw OpenGL.

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
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // wopr_willy_render() finds the window's real size via
    // SDL_GL_GetCurrentWindow(), which only returns something once a real
    // GL context is current. Forcing SDL's "opengl" render driver makes
    // SDL_CreateRenderer create exactly that context under the hood, so
    // resizing/maximizing/fullscreen all report the correct size. If the
    // opengl driver isn't available we fall back to whatever's accelerated,
    // then software — those paths still run, they just always render as if
    // the window were 1280x720 (wopr_willy_render()'s built-in fallback).
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "");
        renderer = SDL_CreateRenderer(
            window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    }
    if (!renderer) {
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
    bool fullscreen = false;
    bool quit_confirm = false;
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
                    if (quit_confirm) {
                        // Game input is frozen while this dialog is up — only
                        // Y/Enter confirm and N/Escape cancel get through.
                        if (ev.key.keysym.sym == SDLK_y || ev.key.keysym.sym == SDLK_RETURN ||
                            ev.key.keysym.sym == SDLK_KP_ENTER) {
                            running = false;
                        } else if (ev.key.keysym.sym == SDLK_n || ev.key.keysym.sym == SDLK_ESCAPE) {
                            quit_confirm = false;
                        }
                        break;
                    }
                    if (ev.key.keysym.sym == SDLK_ESCAPE) {
                        if (wopr_willy_escape_is_ingame(&w)) {
                            wopr_willy_keydown(&w, ev.key.keysym.sym);
                        } else {
                            quit_confirm = true;
                        }
                        break;
                    }
                    if (ev.key.keysym.sym == SDLK_F11) {
                        fullscreen = !fullscreen;
                        SDL_SetWindowFullscreen(window,
                            fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                        break;
                    }
                    wopr_willy_keydown(&w, ev.key.keysym.sym);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (!quit_confirm)
                        wopr_willy_mousedown(&w, ev.button.x, ev.button.y, ev.button.button);
                    break;
                case SDL_MOUSEMOTION:
                    if (!quit_confirm)
                        wopr_willy_mousemove(&w, ev.motion.x, ev.motion.y);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (!quit_confirm)
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

        if (!quit_confirm) wopr_willy_update(&w, dt);  // frozen behind the dialog

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        wopr_willy_render(&w, px, py, cw, ch, cols);

        if (quit_confirm) {
            int ww_, wh_;
            SDL_GetWindowSize(window, &ww_, &wh_);
            const char *msg  = "QUIT WILLY THE WORM?";
            const char *hint = "Y TO QUIT  -  N OR ESC TO CANCEL";
            float box_w = 440.f, box_h = 90.f;
            float box_x = (float)ww_ * 0.5f - box_w * 0.5f;
            float box_y = (float)wh_ * 0.5f - box_h * 0.5f;
            gl_draw_rect(box_x, box_y, box_w, box_h, 0.f, 0.f, 0.f, 0.85f);
            gl_draw_rect(box_x, box_y, box_w, 2.f, 1.f, 1.f, 1.f, 1.f);
            gl_draw_rect(box_x, box_y + box_h - 2.f, box_w, 2.f, 1.f, 1.f, 1.f, 1.f);
            gl_draw_text(msg,  box_x + (box_w - gl_text_width(msg, 1.f)) * 0.5f,
                         box_y + 24.f, 1.f, 1.f, 0.f, 1.f, 1.f);
            gl_draw_text(hint, box_x + (box_w - gl_text_width(hint, 1.f)) * 0.5f,
                         box_y + 54.f, 0.7f, 0.7f, 0.7f, 1.f, 1.f);
            gl_flush_verts();
        }

        SDL_RenderPresent(renderer);
    }

    wopr_willy_free(&w);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
