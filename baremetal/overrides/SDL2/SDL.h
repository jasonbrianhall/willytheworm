// Minimal SDL2 stand-in for the bare-metal build.
//
// wopr_willy.cpp only needs keycodes, modifier state, a mono audio device and
// the window size. This header declares exactly that; sdl_shim.cpp implements
// it on top of the kernel's framebuffer, keyboard queue and HDA/AC97 driver.
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef uint8_t  Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef int16_t  Sint16;
typedef int32_t  Sint32;

// ---------------------------------------------------------------- keyboard
typedef int32_t SDL_Keycode;
enum {
    SDLK_UNKNOWN = 0,
    SDLK_BACKSPACE = 8, SDLK_TAB = 9, SDLK_RETURN = 13, SDLK_ESCAPE = 27,
    SDLK_SPACE = ' ', SDLK_MINUS = '-',
    SDLK_0 = '0', SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9,
    SDLK_LEFTBRACKET = '[', SDLK_RIGHTBRACKET = ']',
    SDLK_a = 'a', SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f, SDLK_g, SDLK_h, SDLK_i, SDLK_j,
    SDLK_k, SDLK_l, SDLK_m, SDLK_n, SDLK_o, SDLK_p, SDLK_q, SDLK_r, SDLK_s, SDLK_t,
    SDLK_u, SDLK_v, SDLK_w, SDLK_x, SDLK_y, SDLK_z,
};
#define SDLK_SCANCODE_MASK (1 << 30)
enum {
    SDLK_F1 = 58 | SDLK_SCANCODE_MASK, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6,
    SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10, SDLK_F11, SDLK_F12,
    SDLK_RIGHT = 79 | SDLK_SCANCODE_MASK, SDLK_LEFT, SDLK_DOWN, SDLK_UP,
    SDLK_KP_ENTER = 88 | SDLK_SCANCODE_MASK,
};

typedef enum {
    KMOD_NONE = 0,
    KMOD_LSHIFT = 0x0001, KMOD_RSHIFT = 0x0002,
    KMOD_LCTRL = 0x0040, KMOD_RCTRL = 0x0080,
    KMOD_LALT = 0x0100, KMOD_RALT = 0x0200,
    KMOD_CTRL = KMOD_LCTRL | KMOD_RCTRL,
    KMOD_SHIFT = KMOD_LSHIFT | KMOD_RSHIFT,
    KMOD_ALT = KMOD_LALT | KMOD_RALT,
} SDL_Keymod;
SDL_Keymod SDL_GetModState(void);

// ---------------------------------------------------------------- mouse / cursor (no-ops)
#define SDL_BUTTON_LEFT   1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT  3
typedef enum { SDL_SYSTEM_CURSOR_ARROW, SDL_SYSTEM_CURSOR_HAND } SDL_SystemCursor;
typedef struct SDL_Cursor SDL_Cursor;
static inline SDL_Cursor* SDL_CreateSystemCursor(SDL_SystemCursor) { return nullptr; }
static inline void SDL_SetCursor(SDL_Cursor*) {}
static inline int SDL_OpenURL(const char*) { return -1; }

// ---------------------------------------------------------------- window
typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
SDL_Window* SDL_GL_GetCurrentWindow(void);
void SDL_GetWindowSize(SDL_Window* window, int* w, int* h);

// ---------------------------------------------------------------- audio
typedef uint32_t SDL_AudioDeviceID;
typedef uint16_t SDL_AudioFormat;
#define AUDIO_S16LSB 0x8010
#define AUDIO_S16SYS AUDIO_S16LSB
typedef void (*SDL_AudioCallback)(void* userdata, Uint8* stream, int len);
typedef struct SDL_AudioSpec {
    int freq;
    SDL_AudioFormat format;
    Uint8 channels;
    Uint8 silence;
    Uint16 samples;
    Uint16 padding;
    Uint32 size;
    SDL_AudioCallback callback;
    void* userdata;
} SDL_AudioSpec;
SDL_AudioDeviceID SDL_OpenAudioDevice(const char* device, int iscapture, const SDL_AudioSpec* desired,
                                      SDL_AudioSpec* obtained, int allowed_changes);
void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on);
void SDL_CloseAudioDevice(SDL_AudioDeviceID dev);
// The "audio thread" is the main loop, so there's nothing to lock against.
static inline void SDL_LockAudioDevice(SDL_AudioDeviceID) {}
static inline void SDL_UnlockAudioDevice(SDL_AudioDeviceID) {}
