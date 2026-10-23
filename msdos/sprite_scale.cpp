// Sprite scaling for the DOS build: the game's 8x8 sprites are enlarged
// with Scale2x / Scale3x (the EPX family of pixel-art scalers, chained for
// 4x, 6x, 8x, 9x...) instead of as blocks, so diagonals come out smooth.
// xBRZ does the same job but is GPL; these rules are short enough to write
// out here. Each sprite is scaled once per size and kept.
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <map>
#include <vector>
#include "video.hpp"

namespace {

typedef std::vector<uint8_t> Bitmap;           // n x n, one byte (0/1) a pixel

// Pixel at (x, y), with the edge repeated outside, so lines that run off a
// cell stay straight there instead of being rounded off.
inline uint8_t at(const Bitmap& s, int n, int x, int y) {
    x = x < 0 ? 0 : x >= n ? n - 1 : x;
    y = y < 0 ? 0 : y >= n ? n - 1 : y;
    return s[y * n + x];
}

// Both scalers work on 1-bit sprites and only ever set background pixels:
// an empty pixel between two set neighbours (above and left, say) gets the
// matching corner filled, but only when the pixel diagonally between those
// neighbours is empty too. That is a step in a diagonal line (smoothed); when
// it's set, it's the inside corner of an L (a ladder rung meeting its rail,
// a pipe bend), which stays sharp. Set pixels are never shaved off, so solid
// blocks stay square.

Bitmap scale2x(const Bitmap& s, int n) {        // EPX / Scale2x
    Bitmap d((size_t)n * n * 4);
    int m = n * 2;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            uint8_t* o = &d[(y * 2) * m + x * 2];
            uint8_t P = at(s, n, x, y);
            if (P) { o[0] = o[1] = o[m] = o[m + 1] = 1; continue; }
            bool A = at(s, n, x, y - 1), B = at(s, n, x + 1, y), C = at(s, n, x - 1, y), D = at(s, n, x, y + 1);
            bool ul = at(s, n, x - 1, y - 1), ur = at(s, n, x + 1, y - 1),
                 dl = at(s, n, x - 1, y + 1), dr = at(s, n, x + 1, y + 1);
            o[0]     = C && A && !D && !B && !ul;
            o[1]     = A && B && !C && !D && !ur;
            o[m]     = D && C && !B && !A && !dl;
            o[m + 1] = B && D && !A && !C && !dr;
        }
    return d;
}

Bitmap scale3x(const Bitmap& s, int n) {        // AdvanceMAME Scale3x
    Bitmap d((size_t)n * n * 9);
    int m = n * 3;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            bool A = at(s, n, x - 1, y - 1), B = at(s, n, x, y - 1), C = at(s, n, x + 1, y - 1),
                 D = at(s, n, x - 1, y),     E = at(s, n, x, y),     F = at(s, n, x + 1, y),
                 G = at(s, n, x - 1, y + 1), H = at(s, n, x, y + 1), I = at(s, n, x + 1, y + 1);
            uint8_t e[9] = {E, E, E, E, E, E, E, E, E};
            if (!E && B != H && D != F) {
                e[0] = D && B && !A;
                e[1] = (D && B && C && !A) || (B && F && A && !C);
                e[2] = B && F && !C;
                e[3] = (D && B && G && !A) || (D && H && A && !G);
                e[5] = (B && F && I && !C) || (H && F && C && !I);
                e[6] = D && H && !G;
                e[7] = (D && H && I && !G) || (H && F && G && !I);
                e[8] = H && F && !I;
            }
            for (int j = 0; j < 3; j++)
                memcpy(&d[(y * 3 + j) * m + x * 3], &e[j * 3], 3);
        }
    return d;
}

// The sprite at k times its size (k made of 2s and 3s), from the cache.
const Bitmap& scaled(const uint8_t rows[8], int k, int& n) {
    static std::map<std::pair<uint64_t, int>, Bitmap> cache;
    uint64_t bits = 0;
    for (int i = 0; i < 8; i++) bits = bits << 8 | rows[i];
    std::pair<uint64_t, int> key(bits, k);
    n = 8 * k;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    Bitmap b(64);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) b[y * 8 + x] = (rows[y] >> (7 - x)) & 1;
    int cur = 8, left = k;
    while (left % 3 == 0 && left > 1) { b = scale3x(b, cur); cur *= 3; left /= 3; }
    while (left % 2 == 0 && left > 1) { b = scale2x(b, cur); cur *= 2; left /= 2; }
    return cache[key] = b;
}

// Smallest scale made of 2s and 3s that reaches `need`.
int pick_k(int need) {
    static const int ks[] = {1, 2, 3, 4, 6, 8, 9, 12, 16, 18, 24, 27, 32};
    for (int k : ks) if (k >= need) return k;
    return 32;
}

inline int iround(float v) { return (int)(v < 0 ? v - 0.5f : v + 0.5f); }
inline uint32_t chan(float c) { return c <= 0.f ? 0 : c >= 1.f ? 255 : (uint32_t)(c * 255.f + 0.5f); }

} // namespace

void gl_draw_sprite8(const uint8_t rows[8], float x, float y, float w, float h,
                     float r, float g, float b, float a) {
    int x0 = iround(x), y0 = iround(y), sw = iround(x + w) - x0, sh = iround(y + h) - y0;
    if (sw <= 0 || sh <= 0 || a <= 0.f) return;
    int big = sw > sh ? sw : sh;
    int n;
    const Bitmap& bm = scaled(rows, pick_k((big + 7) / 8), n);
    uint32_t rgb = chan(r) << 16 | chan(g) << 8 | chan(b);
    uint32_t al = chan(a);
    for (int ty = 0; ty < sh; ty++) {
        int py = y0 + ty;
        if (py < 0 || py >= (int)back_h) continue;
        const uint8_t* src = &bm[(size_t)(ty * n / sh) * n];
        uint32_t* row = back + (size_t)py * back_w;
        for (int tx = 0; tx < sw; tx++) {
            int px = x0 + tx;
            if (px < 0 || px >= (int)back_w || !src[tx * n / sw]) continue;
            if (al >= 255) { row[px] = rgb; continue; }
            uint32_t d = row[px], ia = 255 - al;
            row[px] = (((rgb >> 16) & 255) * al + ((d >> 16) & 255) * ia) / 255 << 16 |
                      (((rgb >> 8) & 255) * al + ((d >> 8) & 255) * ia) / 255 << 8 |
                      ((rgb & 255) * al + (d & 255) * ia) / 255;
        }
    }
}
