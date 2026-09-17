// icon.h — window icon for Willy the Worm, embedded as raw RGBA pixel data
// (see icon_data.h, generated from icon.png resized to 256x256).
#pragma once

#include <SDL2/SDL.h>

// Decodes the embedded icon and returns a new SDL_Surface (256x256, RGBA8888)
// ready for SDL_SetWindowIcon(). Returns nullptr if decoding fails (caller
// should treat this as non-fatal — the game runs fine without a window icon).
// Caller owns the returned surface and must SDL_FreeSurface() it once done
// (SDL_SetWindowIcon copies the pixel data internally, so it's safe to free
// the surface right after the call).
SDL_Surface *load_window_icon();
