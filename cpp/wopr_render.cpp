#include "wopr_render.h"
#include "DejaVuMono.h"   // embedded DejaVu Sans Mono (regular), base64: DEJAVU_REGULAR_FONT_B64

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

static SDL_Renderer *g_renderer = nullptr;

// ============================================================================
// Embedded fallback glyphs
//
// DejaVu Sans Mono covers plain ASCII fine, but just in case a codepoint
// isn't in the font (or FreeType fails to load at all), fall back to a tiny
// hand-drawn 8x8 bitmap set: the four arrow glyphs the intro screen's control
// hints use (U+2190..U+2193 — DejaVu almost certainly has these too, but
// better safe than a silent '?'), plus a '?' placeholder for anything else.
// Same bit convention throughout this file: bit 0 = leftmost pixel in a row.
// ============================================================================
static const unsigned char GLYPH_ARROW_LEFT[8]  = {0x00,0x10,0x18,0x3E,0x18,0x10,0x00,0x00};
static const unsigned char GLYPH_ARROW_UP[8]    = {0x08,0x1C,0x3E,0x08,0x08,0x08,0x08,0x00};
static const unsigned char GLYPH_ARROW_RIGHT[8] = {0x00,0x08,0x18,0x7C,0x18,0x08,0x00,0x00};
static const unsigned char GLYPH_ARROW_DOWN[8]  = {0x08,0x08,0x08,0x08,0x3E,0x1C,0x08,0x00};
static const unsigned char GLYPH_QUESTION[8]    = {0x3C,0x66,0x30,0x18,0x18,0x00,0x18,0x00};

static const unsigned char *fallback_glyph(uint32_t cp) {
    switch (cp) {
        case 0x2190: return GLYPH_ARROW_LEFT;
        case 0x2191: return GLYPH_ARROW_UP;
        case 0x2192: return GLYPH_ARROW_RIGHT;
        case 0x2193: return GLYPH_ARROW_DOWN;
        default:     return GLYPH_QUESTION;
    }
}

// ============================================================================
// UTF-8 decoding
// ============================================================================

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
    return 0xFFFD;
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

// ============================================================================
// Minimal base64 decoder
//
// DejaVuMono.h documents "base64_decode(...)" as the intended way to decode
// DEJAVU_REGULAR_FONT_B64, but that helper actually lives in a sibling header
// (DejaVuMonoBold.h) we're not using here, so it's reimplemented locally —
// same algorithm, just self-contained.
// ============================================================================
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

// ============================================================================
// FreeType state
// ============================================================================

static FT_Library g_ft_lib   = nullptr;
static FT_Face    g_ft_face  = nullptr;
static unsigned char *g_font_data = nullptr;  // must outlive g_ft_face

static int g_cell_w = 10, g_cell_h = 18;  // monospace cell, set for real once the font loads
static int g_ascent = 14;                 // pixels from cell top to baseline

static const int FONT_PIXEL_HEIGHT = 20;  // glyph rasterization size — tweak to taste

static bool load_font() {
    if (FT_Init_FreeType(&g_ft_lib) != 0) return false;

    std::vector<unsigned char> decoded;
    if (!base64_decode(DEJAVU_REGULAR_FONT_B64,
                        sizeof(DEJAVU_REGULAR_FONT_B64) - 1, decoded))
        return false;

    // FreeType needs this buffer alive for as long as the face is — keep a
    // persistent heap copy rather than pointing at the temporary vector.
    g_font_data = (unsigned char *)malloc(decoded.size());
    if (!g_font_data) return false;
    memcpy(g_font_data, decoded.data(), decoded.size());

    if (FT_New_Memory_Face(g_ft_lib, g_font_data, (FT_Long)decoded.size(), 0, &g_ft_face) != 0) {
        free(g_font_data); g_font_data = nullptr;
        return false;
    }

    FT_Set_Pixel_Sizes(g_ft_face, 0, FONT_PIXEL_HEIGHT);

    // Monospace: use 'M's advance as the fixed cell width.
    if (FT_Load_Char(g_ft_face, 'M', FT_LOAD_DEFAULT) == 0)
        g_cell_w = (int)(g_ft_face->glyph->advance.x >> 6);
    if (g_cell_w < 1) g_cell_w = FONT_PIXEL_HEIGHT * 3 / 5;

    g_ascent = (int)(g_ft_face->size->metrics.ascender >> 6);
    int descent = (int)(-(g_ft_face->size->metrics.descender >> 6));
    g_cell_h = g_ascent + descent;
    if (g_cell_h < 1) g_cell_h = FONT_PIXEL_HEIGHT;

    return true;
}

// ============================================================================
// Glyph texture cache
// ============================================================================

struct GlyphTex {
    SDL_Texture *tex = nullptr;
    int w = 0, h = 0;
    int bearing_x = 0, bearing_y = 0;  // FreeType-style: left offset, height above baseline
};

static std::unordered_map<uint32_t, GlyphTex> g_glyph_cache;

static SDL_Texture *make_alpha_texture(const uint8_t *coverage, int w, int h, int pitch) {
    std::vector<uint32_t> pixels((size_t)w * h);
    for (int y = 0; y < h; y++) {
        const uint8_t *row = coverage + (size_t)y * pitch;
        for (int x = 0; x < w; x++)
            pixels[(size_t)y * w + x] = ((uint32_t)row[x] << 24) | 0x00FFFFFFu;
    }
    SDL_Texture *tex = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_STATIC, w, h);
    if (!tex) return nullptr;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(tex, nullptr, pixels.data(), w * 4);
    return tex;
}

static const GlyphTex &get_glyph(uint32_t cp) {
    auto it = g_glyph_cache.find(cp);
    if (it != g_glyph_cache.end()) return it->second;

    GlyphTex g;
    bool got_ft_glyph = false;

    if (g_ft_face && FT_Get_Char_Index(g_ft_face, cp) != 0 &&
        FT_Load_Char(g_ft_face, cp, FT_LOAD_RENDER) == 0) {
        FT_GlyphSlot slot = g_ft_face->glyph;
        FT_Bitmap &bm = slot->bitmap;
        if (bm.width > 0 && bm.rows > 0) {
            SDL_Texture *tex = make_alpha_texture(bm.buffer, (int)bm.width, (int)bm.rows, bm.pitch);
            if (tex) {
                g.tex = tex; g.w = (int)bm.width; g.h = (int)bm.rows;
                g.bearing_x = slot->bitmap_left; g.bearing_y = slot->bitmap_top;
            }
        }
        got_ft_glyph = true;  // even a zero-size bitmap (e.g. space) is a real answer
    }

    if (!got_ft_glyph) {
        // FreeType doesn't have this codepoint at all — use the hand-drawn
        // fallback, upscaled into a texture the same way so it composites
        // identically to a real glyph.
        const unsigned char *bits = fallback_glyph(cp);
        uint8_t coverage[8 * 8];
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                coverage[y * 8 + x] = (bits[y] & (1 << x)) ? 0xFF : 0x00;
        SDL_Texture *tex = make_alpha_texture(coverage, 8, 8, 8);
        if (tex) {
            g.tex = tex; g.w = 8; g.h = 8;
            g.bearing_x = 0; g.bearing_y = g_ascent - 2;
        }
    }

    auto res = g_glyph_cache.emplace(cp, g);
    return res.first->second;
}

// ============================================================================
// Public API
// ============================================================================

bool gl_render_init(SDL_Renderer *renderer) {
    g_renderer = renderer;
    if (g_renderer) SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    return load_font();
}

void gl_render_shutdown() {
    for (auto &kv : g_glyph_cache)
        if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
    g_glyph_cache.clear();
    if (g_ft_face) { FT_Done_Face(g_ft_face); g_ft_face = nullptr; }
    if (g_ft_lib)  { FT_Done_FreeType(g_ft_lib); g_ft_lib = nullptr; }
    if (g_font_data) { free(g_font_data); g_font_data = nullptr; }
}

int gl_font_cell_size() { return g_cell_w; }

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
    Uint8 R = (Uint8)(r * 255.f), G = (Uint8)(g * 255.f), B = (Uint8)(b * 255.f), A = (Uint8)(a * 255.f);

    float cx = x;
    const unsigned char *p = reinterpret_cast<const unsigned char*>(text);
    while (*p) {
        int consumed;
        uint32_t cp = utf8_decode(p, &consumed);
        p += consumed;

        const GlyphTex &gt = get_glyph(cp);
        if (gt.tex && gt.w > 0 && gt.h > 0) {
            SDL_SetTextureColorMod(gt.tex, R, G, B);
            SDL_SetTextureAlphaMod(gt.tex, A);
            float dx = cx + gt.bearing_x * scale;
            float dy = y + (g_ascent - gt.bearing_y) * scale;
            SDL_FRect dst{ dx, dy, gt.w * scale, gt.h * scale };
            SDL_RenderCopyF(g_renderer, gt.tex, nullptr, &dst);
        }
        cx += g_cell_w * scale;
    }
}

float gl_text_width(const char *text, float scale) {
    if (!text) return 0.f;
    return (float)glyph_count(text) * g_cell_w * scale;
}

float gl_text_height(float scale) {
    return (float)g_cell_h * scale;
}

void gl_flush_verts() {
    // SDL_Renderer draws happen immediately, so there's nothing to batch.
    // Present the frame yourself once per loop iteration (see willy_main.cpp).
}
