#include "icon.h"
#include "icon_data.h"

#include <cstdlib>
#include <cstring>
#include <vector>

// Same self-contained base64 decoder as wopr_render.cpp (the DejaVuMono.h
// comment's "base64_decode(...)" helper lives in a sibling header we don't
// use here, so it's reimplemented locally rather than pulled in).
static bool base64_decode(const char *src, size_t src_len, std::vector<unsigned char> &out) {
    out.clear();
    out.reserve((src_len * 3) / 4 + 4);
    unsigned int bits = 0;
    int bitcount = 0;
    for (size_t i = 0; i < src_len; i++) {
        char c = src[i];
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        int val = -1;
        if (c >= 'A' && c <= 'Z') val = c - 'A';
        else if (c >= 'a' && c <= 'z') val = c - 'a' + 26;
        else if (c >= '0' && c <= '9') val = c - '0' + 52;
        else if (c == '+') val = 62;
        else if (c == '/') val = 63;
        else if (c == '=') break;
        else continue;
        bits = (bits << 6) | (unsigned int)val;
        bitcount += 6;
        if (bitcount >= 8) {
            bitcount -= 8;
            out.push_back((unsigned char)((bits >> bitcount) & 0xFF));
        }
    }
    return !out.empty();
}

SDL_Surface *load_window_icon() {
    std::vector<unsigned char> pixels;
    if (!base64_decode(ICON_RGBA_256_B64, sizeof(ICON_RGBA_256_B64) - 1, pixels))
        return nullptr;

    const size_t expected = (size_t)ICON_W * (size_t)ICON_H * 4;
    if (pixels.size() != expected)
        return nullptr;

    // SDL_CreateRGBSurfaceFrom() doesn't copy the pixel buffer, so we hand it
    // a persistent heap copy and let the surface own it (SDL_FreeSurface()
    // frees pixels it doesn't recognize as externally-owned only if we set
    // that up — simplest is to just leak-free it ourselves via a copy SDL
    // *does* own: SDL_CreateRGBSurfaceWithFormat + manual memcpy).
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(
        0, ICON_W, ICON_H, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf) return nullptr;

    SDL_LockSurface(surf);
    std::memcpy(surf->pixels, pixels.data(), expected);
    SDL_UnlockSurface(surf);

    return surf;
}
