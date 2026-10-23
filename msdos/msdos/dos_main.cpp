// MS-DOS (DJGPP) frontend for Willy the Worm.
//
// Plays the part of baremetal/kernel.cpp: runs the unchanged
// ../cpp/wopr_willy.cpp, drawing through ../baremetal/render.cpp into a back
// buffer that is copied to the screen each frame. DOS and DJGPP supply the C
// library, files (scores.txt in the current directory) and memory; this file
// supplies:
//
//   video     VESA VBE 2.0 linear framebuffer (8/15/16/24/32 bpp), a banked
//             VBE 1.2 window without one, or VGA mode 12h (640x480, 16
//             colours) with no VESA BIOS. WILLYVID=WxHxBPP picks a mode.
//   keyboard  BIOS (INT 16h): key presses, as the original
//   sound     PC speaker, as the original: the game's square-wave stream is
//             turned back into tones and played from IRQ0, sped up to
//             ~1165 Hz (the BIOS still gets its 18.2 Hz)
//   time      the same IRQ0 tick count
#include <SDL2/SDL.h>
#include "video.hpp"
#include "wopr.h"
#include "wopr_render.h"

#include <crt0.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <dos.h>
#include <bios.h>
#include <sys/farptr.h>
#include <sys/movedata.h>
#include <sys/exceptn.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Everything the timer interrupt touches must stay in memory.
int _crt0_startup_flags = _CRT0_FLAG_LOCK_MEMORY;

// ---------------------------------------------------------------- timer
// PIT channel 0 at 1193182 / 1024 Hz; every 64th tick goes on to the BIOS
// handler, which keeps the time of day, the floppy motor and INT 1Ch going.
static const uint32_t PIT_HZ = 1193182;
static const uint32_t TICK_DIV = 1024;
static const double TICK_HZ = (double)PIT_HZ / TICK_DIV;

static volatile uint32_t ticks;
static _go32_dpmi_seginfo old_irq0, new_irq0;
static struct __attribute__((packed)) { uint32_t off; uint16_t sel; } old_vec;
static bool timer_on;

// Tone queue, filled by the main loop: PIT divisor (0 = silent) and length
// in PIT counts.
struct Seg { uint16_t div; int32_t counts; };
static const int QN = 4096;
static Seg q[QN];
static volatile int q_head, q_tail;
static volatile int32_t seg_left;          // counts left of the playing segment
static volatile uint16_t spk_div;          // what the speaker is playing

// A new pitch while one is playing is just a new count: the 8254 takes it
// at the end of the half-cycle, so glides stay smooth.
static inline void speaker_set(uint16_t div) {
    if (div == spk_div) return;
    uint16_t was = spk_div;
    spk_div = div;
    if (!div) { outportb(0x61, inportb(0x61) & ~3); return; }
    if (!was) outportb(0x43, 0xB6);
    outportb(0x42, div & 0xFF);
    outportb(0x42, div >> 8);
    if (!was) outportb(0x61, inportb(0x61) | 3);
}

static void timer_isr() {
    ticks++;
    int32_t left = seg_left - (int32_t)TICK_DIV;
    uint16_t div = spk_div;
    while (left <= 0 && q_head != q_tail) {
        left += q[q_head].counts;
        div = q[q_head].div;
        q_head = (q_head + 1) % QN;
    }
    if (left <= 0) { left = 0; div = 0; }   // ran dry: silence
    seg_left = left;
    speaker_set(div);
    if ((ticks & 63) == 0)
        __asm__ __volatile__("pushfl\n\tlcall *%0" :: "m"(old_vec) : "memory", "cc");
    else
        outportb(0x20, 0x20);
}

static void pit_rate(uint32_t div) {
    outportb(0x43, 0x36);
    outportb(0x40, div & 0xFF);
    outportb(0x40, (div >> 8) & 0xFF);
}

static void timer_init() {
    _go32_dpmi_get_protected_mode_interrupt_vector(8, &old_irq0);
    old_vec.off = old_irq0.pm_offset;
    old_vec.sel = old_irq0.pm_selector;
    new_irq0.pm_offset = (unsigned long)timer_isr;
    new_irq0.pm_selector = _go32_my_cs();
    _go32_dpmi_allocate_iret_wrapper(&new_irq0);
    disable();
    _go32_dpmi_set_protected_mode_interrupt_vector(8, &new_irq0);
    pit_rate(TICK_DIV);
    enable();
    timer_on = true;
}

static void timer_shutdown() {
    if (!timer_on) return;
    disable();
    pit_rate(0);                                    // 65536: 18.2 Hz
    _go32_dpmi_set_protected_mode_interrupt_vector(8, &old_irq0);
    enable();
    _go32_dpmi_free_iret_wrapper(&new_irq0);
    spk_div = 1;
    speaker_set(0);
    timer_on = false;
}

// ---------------------------------------------------------------- sound
// wopr_willy.cpp asks SDL for 22050 Hz S16 mono and fills it with square
// waves (ww_beep). Its stream is pulled here as time passes and turned back
// into tones by measuring the half-periods; the speaker plays them.
static SDL_AudioCallback aud_cb;
static void* aud_user;
static int aud_rate = 22050;
static bool aud_open, aud_running;
static bool sound_off;                              // WILLYSND=OFF

SDL_AudioDeviceID SDL_OpenAudioDevice(const char*, int, const SDL_AudioSpec* want, SDL_AudioSpec* got, int) {
    if (!want || !want->callback || sound_off) return 0;
    aud_cb = want->callback;
    aud_user = want->userdata;
    aud_rate = want->freq > 0 ? want->freq : 22050;
    if (got) { *got = *want; got->freq = aud_rate; got->format = AUDIO_S16SYS; got->channels = 1; }
    aud_open = true;
    return 1;
}
void SDL_PauseAudioDevice(SDL_AudioDeviceID, int pause_on) { aud_running = aud_open && !pause_on; }
void SDL_CloseAudioDevice(SDL_AudioDeviceID) { aud_open = aud_running = false; }

static uint64_t pushed_samples, pushed_counts;      // totals, so rounding never drifts

static void q_push(uint16_t div, uint32_t samples) {
    if (!samples) return;
    pushed_samples += samples;
    uint64_t c = pushed_samples * PIT_HZ / (uint32_t)aud_rate;
    int32_t counts = (int32_t)(c - pushed_counts);
    pushed_counts = c;
    int next = (q_tail + 1) % QN;
    if (next == q_head) return;                     // full: drop
    q[q_tail].div = div;
    q[q_tail].counts = counts;
    __asm__ __volatile__("" ::: "memory");          // the entry before the index
    q_tail = next;
}

// Decoder: runs of same-signed samples are half-periods; similar ones are
// gathered into a tone, flushed every ~2 ms so sweeps keep moving.
// A run much shorter than the tone's half-period is the stub of a tone cut
// short (ww_beep restarts its phase each call, as the 1 ms steps of the
// extra-life sweep do): it counts as time at the tone's pitch.
static int run_sign, run_len;
static uint32_t tone_samples;
static float tone_halves;
static float last_half;                             // half-period of the tone just flushed

static void tone_flush() {
    if (!tone_samples) return;
    float half = tone_halves >= 0.5f ? (float)tone_samples / tone_halves : last_half;
    uint32_t div = 0;
    if (half > 0.f) {                               // else only stubs: too short to tell
        last_half = half;
        float hz = (float)aud_rate / (2.f * half);
        if (hz >= 19.f) div = (uint32_t)((float)PIT_HZ / hz + 0.5f);
        if (div > 65535) div = 0;
    }
    q_push((uint16_t)div, tone_samples);
    tone_samples = 0; tone_halves = 0.f;
}

// Pitch is measured over whole periods (a high run and the low one after
// it), which evens out the 22050 Hz rounding of the half-periods.
static uint32_t pend;                               // first half of the period in progress

static void run_end(int sign, uint32_t len) {
    if (!len) return;
    if (!sign) {
        if (pend) {                                 // the tone ends partway through a period
            float half = tone_halves >= 2.f ? (float)tone_samples / tone_halves : last_half;
            tone_samples += pend;
            tone_halves += half > 0.f ? (float)pend / half : 1.f;
            pend = 0;
        }
        tone_flush();
        last_half = 0.f;
        q_push(0, len);
        return;
    }
    float half = tone_halves >= 2.f ? (float)tone_samples / tone_halves : last_half;
    if (half > 0.f) {
        if ((float)len < half * 0.4f) {             // a stub
            tone_samples += len;
            tone_halves += (float)len / half;
            return;
        }
    }
    if (!pend) { pend = len; return; }
    uint32_t period = pend + len;
    pend = 0;
    if (half > 0.f) {
        float avg = 2.f * half;
        float d = (float)period - avg, tol = avg * 0.06f > 2.01f ? avg * 0.06f : 2.01f;
        if (d > tol || d < -tol) tone_flush();
    }
    tone_samples += period;
    tone_halves += 2.f;
    if (tone_samples >= (uint32_t)aud_rate / 500) tone_flush();
}

static void decode(const int16_t* s, int n) {
    for (int i = 0; i < n; i++) {
        int sg = s[i] > 0 ? 1 : s[i] < 0 ? -1 : 0;
        if (sg == run_sign) { run_len++; continue; }
        run_end(run_sign, run_len);
        run_sign = sg;
        run_len = 1;
    }
    // Hand over what is certain so far: silence, and the finished half-periods.
    if (run_sign == 0) { run_end(0, run_len); run_len = 0; }
    else tone_flush();
}

// Counts queued ahead of the speaker.
static int64_t queued_counts() {
    disable();
    int64_t n = seg_left;
    for (int i = q_head; i != q_tail; i = (i + 1) % QN) n += q[i].counts;
    enable();
    return n;
}

// Keep ~50 ms of sound queued: small enough that a jump beeps with the jump.
static void pump_audio() {
    if (!aud_running || !timer_on) return;
    static const int64_t target = PIT_HZ / 20;
    int64_t have = queued_counts();
    if (have >= target) return;
    int want = (int)((target - have) * aud_rate / PIT_HZ) + 1;
    static int16_t buf[512];
    while (want > 0) {
        int n = want < 512 ? want : 512;
        aud_cb(aud_user, (Uint8*)buf, n * 2);
        decode(buf, n);
        want -= n;
    }
}

// ---------------------------------------------------------------- video
enum VMode { V_NONE, V_LFB, V_BANKED, V_VGA12 };
static VMode vmode = V_NONE;
static uint32_t fb_w, fb_h, fb_pitch;
static int fb_bytes = 4;
static bool fb_indexed;
static uint8_t r_pos = 16, r_len = 8, g_pos = 8, g_len = 8, b_pos = 0, b_len = 8;
static int lfb_sel = -1;
static __dpmi_meminfo lfb_map;
static uint32_t win_seg = 0xA000, win_gran = 65536, win_size = 65536;
static int cur_bank = -1;

uint32_t* back;
uint32_t back_w, back_h;

// 8-bit modes: the first 192 colours get exact palette entries, the rest
// map onto a 4x4x4 cube in 192..255 (as kernel.cpp).
static uint32_t pal_key[1024];
static uint8_t pal_val[1024];
static int pal_n, pal_keys;
static void dac_set(int i, uint32_t c) {
    outportb(0x3C8, (uint8_t)i);
    outportb(0x3C9, (c >> 18) & 63); outportb(0x3C9, (c >> 10) & 63); outportb(0x3C9, (c >> 2) & 63);
}
static uint8_t pal_index(uint32_t c) {
    uint32_t h = (c * 2654435761u) >> 22;
    for (;; h = (h + 1) & 1023) {
        if (pal_key[h] == c) return pal_val[h];
        if (pal_key[h] == 0xFFFFFFFF) break;
    }
    uint8_t idx;
    if (pal_n < 192) { idx = (uint8_t)pal_n++; dac_set(idx, c); }
    else idx = (uint8_t)(192 + ((c >> 22) & 3) * 16 + ((c >> 14) & 3) * 4 + ((c >> 6) & 3));
    if (pal_keys < 900) { pal_key[h] = c; pal_val[h] = idx; pal_keys++; }
    return idx;
}
static void pal_reset() {
    for (auto& k : pal_key) k = 0xFFFFFFFF;
    pal_n = pal_keys = 0;
    for (int i = 0; i < 64; i++)
        dac_set(192 + i, (uint32_t)((i >> 4) * 85) << 16 | (uint32_t)(((i >> 2) & 3) * 85) << 8 | (uint32_t)((i & 3) * 85));
    pal_index(0x000000);
}

static const uint32_t ega16[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF };
static uint8_t nearest16(uint32_t c) {
    static uint32_t ck[512]; static uint8_t cv[512]; static bool init;
    if (!init) { for (auto& k : ck) k = 0xFFFFFFFF; init = true; }
    uint32_t h = (c * 2654435761u) >> 23;
    if (ck[h] == c) return cv[h];
    int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255, best = 0;
    long bd = 1L << 30;
    for (int i = 0; i < 16; i++) {
        int dr = r - (int)((ega16[i] >> 16) & 255), dg = g - (int)((ega16[i] >> 8) & 255), db = b - (int)(ega16[i] & 255);
        long d = 3L * dr * dr + 4L * dg * dg + 2L * db * db;
        if (d < bd) { bd = d; best = i; }
    }
    ck[h] = c; cv[h] = (uint8_t)best;
    return (uint8_t)best;
}

static uint32_t native(uint32_t c) {
    c &= 0xFFFFFF;
    if (fb_indexed) return pal_index(c);
    uint32_t r = c >> 16, g = (c >> 8) & 255, b = c & 255;
    return (r >> (8 - r_len)) << r_pos | (g >> (8 - g_len)) << g_pos | (b >> (8 - b_len)) << b_pos;
}

static void set_bank(int bank) {
    if (bank == cur_bank) return;
    __dpmi_regs r;
    memset(&r, 0, sizeof r);
    r.x.ax = 0x4F05; r.x.bx = 0; r.x.dx = (unsigned short)bank;
    __dpmi_int(0x10, &r);
    cur_bank = bank;
}

static void fb_write(uint32_t off, const void* src, uint32_t n) {
    if (vmode == V_LFB) { movedata(_my_ds(), (unsigned)src, lfb_sel, off, n); return; }
    const uint8_t* s = (const uint8_t*)src;
    while (n) {
        int bank = (int)(off / win_gran);
        uint32_t in = off - (uint32_t)bank * win_gran;
        uint32_t room = win_size - in;
        uint32_t k = n < room ? n : room;
        set_bank(bank);
        dosmemput(s, k, win_seg * 16 + in);
        off += k; s += k; n -= k;
    }
}

static void vga12_row(uint32_t y, const uint32_t* src) {
    uint8_t planes[4][80];
    memset(planes, 0, sizeof planes);
    uint32_t last = 0xFFFFFFFF; uint8_t v = 0;
    for (uint32_t x = 0; x < 640; x++) {
        uint32_t p = src[x] & 0xFFFFFF;
        if (p != last) { last = p; v = nearest16(p); }
        uint8_t bit = (uint8_t)(0x80 >> (x & 7));
        for (int pl = 0; pl < 4; pl++) if (v & (1 << pl)) planes[pl][x >> 3] |= bit;
    }
    for (int pl = 0; pl < 4; pl++) {
        outportw(0x3C4, (uint16_t)(((1 << pl) << 8) | 0x02));
        dosmemput(planes[pl], 80, 0xA0000 + y * 80);
    }
    outportw(0x3C4, 0x0F02);
}

static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static bool vbe_mode_info(int mode, uint8_t mi[256]) {
    __dpmi_regs r;
    memset(&r, 0, sizeof r);
    r.x.ax = 0x4F01; r.x.cx = (unsigned short)mode;
    r.x.es = __tb >> 4; r.x.di = __tb & 15;
    __dpmi_int(0x10, &r);
    if (r.x.ax != 0x004F) return false;
    dosmemget(__tb, 256, mi);
    return true;
}

static bool vesa_init() {
    uint8_t info[512];
    memset(info, 0, sizeof info);
    memcpy(info, "VBE2", 4);
    dosmemput(info, sizeof info, __tb);
    __dpmi_regs r;
    memset(&r, 0, sizeof r);
    r.x.ax = 0x4F00; r.x.es = __tb >> 4; r.x.di = __tb & 15;
    __dpmi_int(0x10, &r);
    if (r.x.ax != 0x004F) return false;
    dosmemget(__tb, sizeof info, info);
    if (memcmp(info, "VESA", 4) != 0) return false;
    int ver = rd16(info + 4);
    uint32_t lp = rd32(info + 14);
    uint32_t list_lin = ((lp >> 16) << 4) + (lp & 0xFFFF);
    uint16_t modes[256];
    int nm = 0;
    for (; nm < 255; nm++) {
        uint16_t m = _farpeekw(_dos_ds, list_lin + nm * 2);
        if (m == 0xFFFF) break;
        modes[nm] = m;
    }

    // WILLYVID=WxHxBPP, otherwise 640x480; 8 bits first (Willy has few
    // colours, and a byte a pixel is the least to push over the bus).
    int want_w = 640, want_h = 480, want_bpp = 0;
    if (const char* e = getenv("WILLYVID")) sscanf(e, "%dx%dx%d", &want_w, &want_h, &want_bpp);
    static const int pref[] = {8, 16, 15, 32, 24};
    int best = -1, best_score = -1;
    uint8_t mi[256];
    for (int i = 0; i < nm; i++) {
        if (!vbe_mode_info(modes[i], mi)) continue;
        uint16_t attr = rd16(mi);
        if ((attr & 0x19) != 0x19) continue;
        int w = rd16(mi + 18), h = rd16(mi + 20), bpp = mi[25], model = mi[27];
        if (w < 640 || h < 480) continue;
        if (bpp == 8 ? model != 4 : model != 6) continue;
        bool lfb = (attr & 0x80) && ver >= 0x200 && rd32(mi + 40);
        if (!lfb && (mi[2] & 5) != 5) continue;
        int score = 0;
        if (w == want_w && h == want_h) score += 1000;
        else score += 500 - abs(w - want_w) / 8 - abs(h - want_h) / 8;
        if (want_bpp) { if (bpp == want_bpp) score += 100; }
        else for (int k = 0; k < 5; k++) if (pref[k] == bpp) score += 100 - k * 10;
        if (lfb) score += 5;
        if (score > best_score) { best_score = score; best = modes[i]; }
    }
    if (best < 0 || !vbe_mode_info(best, mi)) return false;

    uint16_t attr = rd16(mi);
    fb_w = rd16(mi + 18); fb_h = rd16(mi + 20);
    int bpp = mi[25];
    fb_bytes = (bpp + 7) / 8;
    fb_indexed = bpp == 8;
    fb_pitch = rd16(mi + 16);
    if (!fb_indexed) {
        r_len = mi[31]; r_pos = mi[32]; g_len = mi[33]; g_pos = mi[34]; b_len = mi[35]; b_pos = mi[36];
        if (!r_len || !g_len || !b_len) {
            if (bpp == 15) { r_pos = 10; r_len = 5; g_pos = 5; g_len = 5; b_pos = 0; b_len = 5; }
            else if (bpp == 16) { r_pos = 11; r_len = 5; g_pos = 5; g_len = 6; b_pos = 0; b_len = 5; }
            else { r_pos = 16; r_len = 8; g_pos = 8; g_len = 8; b_pos = 0; b_len = 8; }
        }
    }
    bool lfb = (attr & 0x80) && ver >= 0x200 && rd32(mi + 40);
    if (lfb) {
        if (ver >= 0x300 && rd16(mi + 50)) fb_pitch = rd16(mi + 50);
        lfb_map.address = rd32(mi + 40);
        lfb_map.size = (fb_pitch * fb_h + 0xFFF) & ~0xFFFu;
        if (__dpmi_physical_address_mapping(&lfb_map) != 0) lfb = false;
        else {
            lfb_sel = __dpmi_allocate_ldt_descriptors(1);
            if (lfb_sel < 0) lfb = false;
            else {
                __dpmi_set_segment_base_address(lfb_sel, lfb_map.address);
                __dpmi_set_segment_limit(lfb_sel, (lfb_map.size - 1) | 0xFFF);
            }
        }
    }
    if (!lfb) {
        if ((mi[2] & 5) != 5) return false;
        win_gran = (uint32_t)rd16(mi + 4) * 1024;
        win_size = (uint32_t)rd16(mi + 6) * 1024;
        win_seg = rd16(mi + 8);
        if (!win_gran || !win_size || !win_seg) return false;
    }
    memset(&r, 0, sizeof r);
    r.x.ax = 0x4F02; r.x.bx = (unsigned short)(best | (lfb ? 0x4000 : 0));
    __dpmi_int(0x10, &r);
    if (r.x.ax != 0x004F) return false;
    vmode = lfb ? V_LFB : V_BANKED;
    cur_bank = -1;
    return true;
}

static void vga12_init() {
    __dpmi_regs r;
    memset(&r, 0, sizeof r);
    r.x.ax = 0x0012;
    __dpmi_int(0x10, &r);
    fb_w = 640; fb_h = 480; fb_pitch = 80; fb_bytes = 1; fb_indexed = false;
    vmode = V_VGA12;
}

static void text_mode() {
    if (vmode == V_NONE) return;
    __dpmi_regs r;
    memset(&r, 0, sizeof r);
    r.x.ax = 0x0003;
    __dpmi_int(0x10, &r);
    vmode = V_NONE;
}

// Copy the back buffer out, skipping rows unchanged since the last frame.
static void present() {
    static uint32_t* row_sum;
    static uint8_t* line;
    static bool have_sums;
    if (!row_sum) {
        row_sum = (uint32_t*)malloc(back_h * sizeof(uint32_t));
        line = (uint8_t*)malloc((size_t)back_w * 4);
        if (!row_sum || !line) return;
    }
    for (uint32_t y = 0; y < back_h; y++) {
        if ((y & 63) == 63) pump_audio();
        const uint32_t* src = &back[y * back_w];
        uint32_t sum = 0x811C9DC5u;
        for (uint32_t x = 0; x < back_w; x++) sum = (sum ^ src[x]) * 16777619u;
        if (have_sums && row_sum[y] == sum) continue;
        row_sum[y] = sum;
        if (vmode == V_VGA12) { vga12_row(y, src); continue; }
        if (fb_bytes == 4 && !fb_indexed && r_pos == 16 && g_pos == 8 && b_pos == 0) {
            fb_write(y * fb_pitch, src, back_w * 4);
            continue;
        }
        uint8_t* d = line;
        uint32_t last = 0xFFFFFFFF, v = 0;
        for (uint32_t x = 0; x < back_w; x++) {
            uint32_t p = src[x];
            if (p != last) { last = p; v = native(p); }
            switch (fb_bytes) {
            case 1: *d++ = (uint8_t)v; break;
            case 2: *(uint16_t*)d = (uint16_t)v; d += 2; break;
            case 3: d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); d[2] = (uint8_t)(v >> 16); d += 3; break;
            default: *(uint32_t*)d = v; d += 4; break;
            }
        }
        fb_write(y * fb_pitch, line, back_w * fb_bytes);
    }
    have_sums = true;
}

// ---------------------------------------------------------------- SDL: window
static int dummy_window;
SDL_Window* SDL_GL_GetCurrentWindow(void) { return (SDL_Window*)&dummy_window; }
void SDL_GetWindowSize(SDL_Window*, int* w, int* h) {
    if (w) *w = (int)back_w;
    if (h) *h = (int)back_h;
}

// ---------------------------------------------------------------- SDL: keyboard
static uint32_t mods;
SDL_Keymod SDL_GetModState(void) { return (SDL_Keymod)mods; }

static void read_mods() {
    int f = bioskey(2);
    mods = 0;
    if (f & 1) mods |= KMOD_RSHIFT;
    if (f & 2) mods |= KMOD_LSHIFT;
    if (f & 4) mods |= KMOD_LCTRL;
    if (f & 8) mods |= KMOD_LALT;
}

// getxkey(): ASCII, 0x100 + scan code for extended keys, 0x200 + scan code
// for the grey ones.
static SDL_Keycode map_key(int k) {
    if (k >= 0x100) {
        switch (k & 0xFF) {
        case 0x48: return SDLK_UP;    case 0x50: return SDLK_DOWN;
        case 0x4B: return SDLK_LEFT;  case 0x4D: return SDLK_RIGHT;
        case 0x85: return SDLK_F11;   case 0x86: return SDLK_F12;
        }
        int sc = k & 0xFF;
        if (sc >= 0x3B && sc <= 0x44) return SDLK_F1 + (sc - 0x3B);
        return 0;
    }
    // Ctrl+letter arrives as 1..26 (Ctrl may be up again by the time it's
    // read); Backspace, Tab and Enter share codes with Ctrl+H, I and M.
    if (k >= 1 && k <= 26 && k != 8 && k != 9 && k != 13) {
        mods |= KMOD_LCTRL;
        return SDLK_a + (k - 1);
    }
    if (k >= 'A' && k <= 'Z') return k - 'A' + 'a';
    if (k == 10) return SDLK_RETURN;
    return k;
}

static SDL_Keycode next_key() {
    // A key waiting? Check the BIOS buffer (memory) before INT 16h.
    while (_farpeekw(_dos_ds, 0x41A) != _farpeekw(_dos_ds, 0x41C) && bioskey(0x11)) {
        read_mods();
        if (SDL_Keycode k = map_key(getxkey())) return k;
    }
    return 0;
}

// ---------------------------------------------------------------- quit dialog
static void draw_quit_dialog() {
    const char* msg  = "QUIT WILLY THE WORM?";
    const char* hint = "Y TO EXIT TO DOS  -  N OR ESC TO CANCEL";
    float pad   = 32.f;
    float tw    = gl_text_width(msg, 1.f) > gl_text_width(hint, 1.f) ? gl_text_width(msg, 1.f) : gl_text_width(hint, 1.f);
    float box_w = tw + pad * 2.f;
    float box_h = 90.f;
    float box_x = (float)back_w * 0.5f - box_w * 0.5f;
    float box_y = (float)back_h * 0.5f - box_h * 0.5f;
    gl_draw_rect(box_x, box_y, box_w, box_h, 0.f, 0.f, 0.f, 0.85f);
    gl_draw_rect(box_x, box_y, box_w, 2.f, 1.f, 1.f, 1.f, 1.f);
    gl_draw_rect(box_x, box_y + box_h - 2.f, box_w, 2.f, 1.f, 1.f, 1.f, 1.f);
    gl_draw_text(msg,  box_x + (box_w - gl_text_width(msg, 1.f)) * 0.5f, box_y + 24.f, 1.f, 1.f, 0.f, 1.f, 1.f);
    gl_draw_text(hint, box_x + (box_w - gl_text_width(hint, 1.f)) * 0.5f, box_y + 54.f, 0.7f, 0.7f, 0.7f, 1.f, 1.f);
}

// ---------------------------------------------------------------- start / exit
static void restore_all() {
    timer_shutdown();
    text_mode();
    if (lfb_sel >= 0) { __dpmi_free_ldt_descriptor(lfb_sel); lfb_sel = -1; }
    if (lfb_map.size) { __dpmi_free_physical_address_mapping(&lfb_map); lfb_map.size = 0; }
}
static void on_fault(int sig) {
    restore_all();
    signal(sig, SIG_DFL);
    raise(sig);
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (*a == '/' || *a == '-') a++;
        if (!strcasecmp(a, "nosound") || !strcasecmp(a, "q")) sound_off = true;
        else if (!strcasecmp(a, "vga")) setenv("WILLYVID", "vga", 1);
        else {
            printf("WILLY [/NOSOUND] [/VGA]\n"
                   "  /NOSOUND  no PC speaker\n"
                   "  /VGA      16-colour VGA, skip VESA\n"
                   "  SET WILLYVID=800x600x16 picks a VESA mode\n");
            return 0;
        }
    }
    if (const char* e = getenv("WILLYSND"))
        if (!strcasecmp(e, "off") || !strcmp(e, "0")) sound_off = true;

    __djgpp_set_ctrl_c(0);
    _go32_want_ctrl_break(1);

    const char* vid = getenv("WILLYVID");
    if (!(vid && !strcasecmp(vid, "vga") ) && vesa_init()) {}
    else vga12_init();
    if (fb_indexed) pal_reset();
    atexit(restore_all);
    signal(SIGSEGV, on_fault); signal(SIGFPE, on_fault); signal(SIGILL, on_fault);
    signal(SIGABRT, on_fault);

    back_w = fb_w; back_h = fb_h;
    back = (uint32_t*)malloc((size_t)back_w * back_h * 4);
    if (!back) { restore_all(); printf("Not enough memory for the screen.\n"); return 1; }
    memset(back, 0, (size_t)back_w * back_h * 4);
    render_set_screen(back_w, back_h);
    gl_render_init(nullptr);

    static WoprState w;
    wopr_willy_enter(&w);
    if (!w.sub_state) {
        restore_all();
        printf("Willy failed to start\n");
        for (auto& l : w.lines) printf("%s\n", l.c_str());
        return 1;
    }

    timer_init();

    // The game sizes its 40x25 grid from the screen less the margins. Pick
    // margins that make a cell a whole multiple of 8 px, so every sprite
    // pixel scales by the same amount (sprite_scale.cpp smooths them).
    const int cw = gl_font_cell_size(), ch = gl_font_cell_size();
    int px = 40, py = 40;
    {
        int kw = (int)back_w / (40 * 8);
        int kh = ((int)back_h - 40 - ch * 2 - 8) / (25 * 8);
        int k = kw < kh ? kw : kh;
        if (k >= 1) {
            int cell = 8 * k;
            px = ((int)back_w - 40 * cell) / 2;
            int room = (int)back_h - ch * 2 - 8 - 25 * cell;     // rows above the grid
            py = room < 40 ? room : 40;
        }
    }
    const int cols = ((int)back_w - px * 2) / cw;
    const uint32_t frame_ticks = (uint32_t)(TICK_HZ / 60.0 + 0.5);

    bool quit = false, quit_confirm = false;
    uint32_t last = ticks;
    while (!quit) {
        while (ticks - last < frame_ticks) pump_audio();
        uint32_t now = ticks, elapsed = now - last;
        last = now;
        double dt = (double)elapsed / TICK_HZ;
        if (dt > 0.1) dt = 0.1;

        while (SDL_Keycode k = next_key()) {
            if (quit_confirm) {
                if (k == SDLK_y || k == SDLK_RETURN) { quit = true; break; }
                if (k == SDLK_n || k == SDLK_ESCAPE) quit_confirm = false;
                continue;
            }
            if (k == SDLK_ESCAPE) {
                if (wopr_willy_escape_is_ingame(&w)) wopr_willy_keydown(&w, k);
                else quit_confirm = true;
                continue;
            }
            wopr_willy_keydown(&w, k);
        }
        if (quit) break;

        if (!quit_confirm) wopr_willy_update(&w, dt);
        pump_audio();

        memset(back, 0, (size_t)back_w * back_h * 4);
        wopr_willy_render(&w, px, py, cw, ch, cols);
        if (quit_confirm) draw_quit_dialog();
        present();
    }

    wopr_willy_free(&w);
    restore_all();
    printf("Thanks for playing Willy the Worm!\n");
    return 0;
}
