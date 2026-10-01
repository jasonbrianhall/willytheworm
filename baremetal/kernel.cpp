// Bare-metal frontend for Willy the Worm.
//
// Boots via Multiboot (GRUB or QEMU -kernel) or the UEFI loader, and runs the
// unchanged wopr_willy.cpp game on a framebuffer. This file plays the part of
// willy_main.cpp plus the slice of SDL2 the game uses (see overrides/SDL2):
// PS/2 + USB keyboard input, the window size, and a mono audio device that
// feeds the HD Audio / AC97 driver.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "hw.hpp"
#include "video.hpp"
#include "audio.hpp"
#include "pci.hpp"
#include "usb.hpp"
#include "floppy.hpp"
#include "storage.hpp"
#include "font.h"
#include "wopr.h"
#include "wopr_render.h"
#include <SDL2/SDL.h>

// ---------------------------------------------------------------- serial
#define COM1 0x3F8
extern "C" void serial_putc(char c) {
    if (c == '\n') serial_putc('\r');
    for (int i = 0; i < 100000 && !(inb(COM1 + 5) & 0x20); i++) {}
    outb(COM1, (uint8_t)c);
}
extern "C" void serial_puts(const char* s) { while (*s) serial_putc(*s++); }
static void serial_init() {
    outb(COM1 + 1, 0x00); outb(COM1 + 3, 0x80); outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00); outb(COM1 + 3, 0x03); outb(COM1 + 2, 0xC7);
}

// ---------------------------------------------------------------- multiboot
struct __attribute__((packed)) MultibootInfo {
    uint32_t flags, mem_lower, mem_upper, boot_device, cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length, mmap_addr, drives_length, drives_addr;
    uint32_t config_table, boot_loader_name, apm_table;
    uint32_t vbe_control_info, vbe_mode_info;
    uint16_t vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    uint64_t fb_addr;
    uint32_t fb_pitch, fb_width, fb_height;
    uint8_t fb_bpp, fb_type;
    uint8_t fb_pad[2];      // GRUB aligns what follows to 4 bytes
    uint8_t fb_color[6];    // type 1 (RGB): red, green, blue (position, size) pairs
};
struct __attribute__((packed)) MultibootMmap { uint32_t size; uint64_t addr, len; uint32_t type; };
extern "C" uint32_t mb_magic, mb_info;
extern "C" char __kernel_start[], __kernel_end[];
extern "C" void heap_add(void* p, size_t n);
extern "C" size_t heap_free_bytes(void), heap_peak_bytes(void);
extern "C" uint64_t phys_limit;
uint64_t phys_limit = 0x100000000ull;

// ---------------------------------------------------------------- memory
// Give the heap every usable RAM region the boot loader reports, minus the
// kernel image and anything below 1 MiB. Uses the Multiboot memory map
// (GRUB, QEMU, and the UEFI loader, which translates the UEFI map), else the
// "upper memory" size.
static void heap_init(const MultibootInfo* mbi) {
    const uint64_t k0 = (uintptr_t)__kernel_start & ~0xFFFull;
    const uint64_t k1 = ((uintptr_t)__kernel_end + 0xFFF) & ~0xFFFull;
    uint64_t total = 0;
    auto add = [&](uint64_t a, uint64_t e) {
        if (e > phys_limit) e = phys_limit;
        if (e > (uint64_t)(uintptr_t)-1) e = (uint64_t)(uintptr_t)-1;   // i386: 32-bit pointers
        if (a < 0x100000) a = 0x100000;
        if (a < k1 && e > k0) {                              // skip the kernel image
            if (a < k0) { heap_add((void*)(uintptr_t)a, (size_t)(k0 - a)); total += k0 - a; }
            a = k1;
        }
        if (e > a) { heap_add((void*)(uintptr_t)a, (size_t)(e - a)); total += e - a; }
    };
    if (mbi->flags & (1 << 6)) {
        uintptr_t p = mbi->mmap_addr, end = p + mbi->mmap_length;
        // Copy the map first: the heap may be handed the memory it sits in.
        static MultibootMmap map[128];
        int n = 0;
        while (p < end && n < 128) {
            const MultibootMmap* m = (const MultibootMmap*)p;
            map[n++] = *m;
            p += m->size + 4;
        }
        for (int i = 0; i < n; i++)
            if (map[i].type == 1) add(map[i].addr, map[i].addr + map[i].len);
    } else if (mbi->flags & 1) {
        add(0x100000, 0x100000 + (uint64_t)mbi->mem_upper * 1024);
    }
    printf("Heap: %lu MB of RAM\n", (unsigned long)(total >> 20));
}

// ---------------------------------------------------------------- video
// The mode to set when no boot loader set one (QEMU -kernel, via Bochs VBE):
// the desktop build's 1280x720 window, or what the Makefile picks per ARCH.
#ifndef FB_W
#define FB_W 1280
#define FB_H 720
#define FB_BPP 32
#endif

// The game draws into `back` as 0xRRGGBB at the screen's size; present()
// converts it to whatever the framebuffer is: 8 (palettized), 15, 16, 24 or
// 32 bits per pixel. 386-era VESA cards with 1 MB manage 1024x768 only at 8.
static uint8_t* fb;
static uint32_t fb_w, fb_h, fb_pitch;   // pitch in bytes
static int fb_bytes = 4;                // per pixel
static bool fb_indexed;
static uint8_t r_pos = 16, r_len = 8, g_pos = 8, g_len = 8, b_pos = 0, b_len = 8;
uint32_t* back;
uint32_t back_w, back_h;

// 8-bit modes: the first 192 colours to appear get exact palette entries
// (VGA DAC); after that, colours map onto a fixed 4x4x4 cube in entries
// 192..255 (anti-aliased text can make more shades than a palette holds).
static uint32_t pal_key[1024];          // colour -> entry, open addressing
static uint8_t pal_val[1024];
static int pal_n, pal_keys;
static void dac_set(int i, uint32_t c) {
    outb(0x3C8, (uint8_t)i);
    outb(0x3C9, (c >> 18) & 63); outb(0x3C9, (c >> 10) & 63); outb(0x3C9, (c >> 2) & 63);
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
    for (int i = 0; i < 64; i++)                          // the cube: 0, 85, 170, 255 per channel
        dac_set(192 + i, (uint32_t)((i >> 4) * 85) << 16 | (uint32_t)(((i >> 2) & 3) * 85) << 8 | (uint32_t)((i & 3) * 85));
    pal_index(0x000000);                                  // entry 0: black
}
static uint32_t native(uint32_t c) {
    c &= 0xFFFFFF;
    if (fb_indexed) return pal_index(c);
    uint32_t r = c >> 16, g = (c >> 8) & 255, b = c & 255;
    return (r >> (8 - r_len)) << r_pos | (g >> (8 - g_len)) << g_pos | (b >> (8 - b_len)) << b_pos;
}
// Video memory sits on a slow bus (ISA on a 386): move dwords.
static void copy_out(void* d, const void* s, size_t n) {
    size_t words = n / 4, rest = n % 4;
    __asm__ volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(words) :: "memory");
    __asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(rest) :: "memory");
}

static bool bga_init(uint32_t w, uint32_t h, uint32_t bpp) {
    uint32_t base = 0;
    PciDevice vga;
    if (pci_find_id(0x1234, 0x1111, &vga))                  // QEMU/Bochs std VGA
        base = pci_read(vga, 0x10) & 0xFFFFFFF0;
    outw(0x1CE, 0); if (!base || inw(0x1CF) < 0xB0C0) return false;
    auto w16 = [](uint16_t i, uint16_t v) { outw(0x1CE, i); outw(0x1CF, v); };
    w16(4, 0); w16(1, w); w16(2, h); w16(3, bpp); w16(4, 0x41);
    fb = (uint8_t*)(uintptr_t)base;
    fb_bytes = (bpp + 7) / 8;
    fb_w = w; fb_h = h; fb_pitch = w * fb_bytes;
    fb_indexed = bpp == 8;
    if (bpp == 16) { r_pos = 11; r_len = 5; g_pos = 5; g_len = 6; b_pos = 0; b_len = 5; }
    if (bpp == 15) { r_pos = 10; r_len = 5; g_pos = 5; g_len = 5; b_pos = 0; b_len = 5; }
    return true;
}

static bool video_init(const MultibootInfo* mbi) {
    bool ok = false;
    if ((mbi->flags & (1 << 12)) && mbi->fb_addr < phys_limit) {
        int bpp = mbi->fb_bpp;
        if (mbi->fb_type == 0 && bpp == 8) {
            fb_indexed = ok = true;
        } else if (mbi->fb_type == 1 && (bpp == 15 || bpp == 16 || bpp == 24 || bpp == 32)) {
            const uint8_t* c = mbi->fb_color;
            r_pos = c[0]; r_len = c[1]; g_pos = c[2]; g_len = c[3]; b_pos = c[4]; b_len = c[5];
            ok = r_len && r_len <= 8 && g_len && g_len <= 8 && b_len && b_len <= 8;
        }
        if (ok) {
            fb = (uint8_t*)(uintptr_t)mbi->fb_addr;
            fb_w = mbi->fb_width; fb_h = mbi->fb_height; fb_pitch = mbi->fb_pitch;
            fb_bytes = (bpp + 7) / 8;
            printf("Using bootloader framebuffer %ux%u, %d bits\n", fb_w, fb_h, bpp);
        }
    }
    if (!ok && bga_init(FB_W, FB_H, FB_BPP)) {
        printf("Using Bochs/QEMU VBE %ux%u, %d bits\n", FB_W, FB_H, FB_BPP);
        ok = true;
    }
    if (ok && fb_indexed) pal_reset();
    return ok;
}

static void pump_audio_now();                   // below: feeds the sound card

// Copy the back buffer out, skipping rows that haven't changed since the
// last frame (a checksum per row): most of Willy's screen is still.
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
        if ((y & 31) == 31) pump_audio_now();         // a full redraw takes a while: keep the sound fed
        const uint32_t* src = &back[y * back_w];
        uint32_t sum = 0x811C9DC5u;
        for (uint32_t x = 0; x < back_w; x++) sum = (sum ^ src[x]) * 16777619u;
        if (have_sums && row_sum[y] == sum) continue;
        row_sum[y] = sum;
        uint8_t* dst = fb + y * fb_pitch;
        if (fb_bytes == 4 && !fb_indexed && r_pos == 16 && g_pos == 8 && b_pos == 0) {
            copy_out(dst, src, back_w * 4);                // already in the screen's format
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
        copy_out(dst, line, back_w * fb_bytes);
    }
    have_sums = true;
}

// ---------------------------------------------------------------- interrupts
#ifdef __x86_64__
struct __attribute__((packed)) IdtEntry {
    uint16_t off_lo, sel; uint8_t ist, type; uint16_t off_mid; uint32_t off_hi, zero;
};
#else
struct __attribute__((packed)) IdtEntry {         // 32-bit interrupt gate
    uint16_t off_lo, sel; uint8_t zero, type; uint16_t off_hi;
};
#endif
static IdtEntry idt[256];
extern "C" void isr_timer(), isr_keyboard(), isr_spurious(), isr_fault();

static void set_gate(int n, void (*h)()) {
    uintptr_t a = (uintptr_t)h;
#ifdef __x86_64__
    idt[n] = { (uint16_t)a, 0x08, 0, 0x8E, (uint16_t)(a >> 16), (uint32_t)(a >> 32), 0 };
#else
    idt[n] = { (uint16_t)a, 0x08, 0, 0x8E, (uint16_t)(a >> 16) };
#endif
}

extern volatile uint32_t ticks, fine_ticks;
extern volatile uint8_t kbd_buf[256];
extern volatile uint8_t kbd_head, kbd_tail;
#define TICK_HZ 60

static void interrupts_init() {
    for (int i = 0; i < 32; i++) set_gate(i, isr_fault);
    for (int i = 32; i < 256; i++) set_gate(i, isr_spurious);
    set_gate(32, isr_timer);
    set_gate(33, isr_keyboard);
    struct __attribute__((packed)) { uint16_t lim; uintptr_t base; } idtr = { sizeof(idt) - 1, (uintptr_t)idt };
    __asm__ volatile("lidt %0" ::"m"(idtr));

    // Remap the PICs to vectors 32..47; unmask only timer and keyboard.
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 32);   outb(0xA1, 40);
    outb(0x21, 4);    outb(0xA1, 2);
    outb(0x21, 1);    outb(0xA1, 1);
    outb(0x21, 0xFC); outb(0xA1, 0xFF);

    uint16_t div = 1193182 / (TICK_HZ * 4);            // see irq.cpp: 240 Hz, ticks at 60
    outb(0x43, 0x36); outb(0x40, div & 0xFF); outb(0x40, div >> 8);

    for (int i = 0; i < 64 && (inb(0x64) & 1); i++) inb(0x60);
    __asm__ volatile("sti");
}

static void reboot() {
    printf("Rebooting\n");
    __asm__ volatile("cli");
    for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) {}
    outb(0x64, 0xFE);                                  // i8042 pulse reset line
    outb(0xCF9, 0x02); outb(0xCF9, 0x06);              // PCI reset control
    struct __attribute__((packed)) { uint16_t lim; uintptr_t base; } none = { 0, 0 };
    __asm__ volatile("lidt %0; int3" ::"m"(none));     // triple fault
    for (;;) __asm__ volatile("hlt");
}

// ---------------------------------------------------------------- SDL: keyboard
static uint32_t mods;                                  // SDL_Keymod bits
SDL_Keymod SDL_GetModState(void) { return (SDL_Keymod)mods; }

// Set-1 scancode -> SDL keycode (ASCII for printable keys, like SDL).
static SDL_Keycode keycode(bool ext, uint8_t code) {
    if (ext) {
        switch (code) {
        case 0x48: return SDLK_UP;    case 0x50: return SDLK_DOWN;
        case 0x4B: return SDLK_LEFT;  case 0x4D: return SDLK_RIGHT;
        case 0x1C: return SDLK_KP_ENTER;
        }
        return 0;
    }
    static const char row1[] = "1234567890-=";          // 0x02..0x0D
    static const char row2[] = "qwertyuiop[]";          // 0x10..0x1B
    static const char row3[] = "asdfghjkl;'`";          // 0x1E..0x29
    static const char row4[] = "\\zxcvbnm,./";          // 0x2B..0x35
    if (code >= 0x02 && code <= 0x0D) return row1[code - 0x02];
    if (code >= 0x10 && code <= 0x1B) return row2[code - 0x10];
    if (code >= 0x1E && code <= 0x29) return row3[code - 0x1E];
    if (code >= 0x2B && code <= 0x35) return row4[code - 0x2B];
    switch (code) {
    case 0x01: return SDLK_ESCAPE;   case 0x0E: return SDLK_BACKSPACE;
    case 0x0F: return SDLK_TAB;      case 0x1C: return SDLK_RETURN;
    case 0x39: return SDLK_SPACE;
    case 0x48: return SDLK_UP;       case 0x50: return SDLK_DOWN;   // keypad arrows, NumLock off
    case 0x4B: return SDLK_LEFT;     case 0x4D: return SDLK_RIGHT;
    case 0x57: return SDLK_F11;      case 0x58: return SDLK_F12;
    }
    if (code >= 0x3B && code <= 0x44) return SDLK_F1 + (code - 0x3B);
    return 0;
}

// Queue a scancode from a source other than the PS/2 interrupt (USB).
void kbd_push(uint8_t b) {
    uintptr_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    kbd_buf[kbd_head] = b;
    kbd_head = kbd_head + 1;
    __asm__ volatile("push %0; popf" :: "r"(flags) : "memory", "cc");
}

// Returns the next key press (0 when the queue is empty), tracking modifiers.
// Like willy_main.cpp, only presses are delivered; the PS/2 keyboard's own
// typematic repeat produces repeated presses just as SDL does.
static SDL_Keycode next_key() {
    static bool ext;
    while (kbd_tail != kbd_head) {
        uint8_t b = kbd_buf[kbd_tail++];
        if (b == 0xE0) { ext = true; continue; }
        if (b == 0xE1) { ext = false; continue; }
        bool down = !(b & 0x80), e = ext;
        uint8_t code = b & 0x7F;
        ext = false;
        uint32_t bit = 0;
        switch (code) {
        case 0x1D: bit = e ? KMOD_RCTRL : KMOD_LCTRL; break;
        case 0x38: bit = e ? KMOD_RALT : KMOD_LALT; break;
        case 0x2A: if (!e) bit = KMOD_LSHIFT; break;
        case 0x36: if (!e) bit = KMOD_RSHIFT; break;
        }
        if (bit) { if (down) mods |= bit; else mods &= ~bit; continue; }
        if (e && (code == 0x2A || code == 0x36)) continue;   // fake shifts around E0 keys
        if (!down) continue;
        if (SDL_Keycode k = keycode(e, code)) return k;
    }
    return 0;
}

// ---------------------------------------------------------------- SDL: window
static int dummy_window;
SDL_Window* SDL_GL_GetCurrentWindow(void) { return (SDL_Window*)&dummy_window; }
void SDL_GetWindowSize(SDL_Window*, int* w, int* h) {
    if (w) *w = (int)back_w;
    if (h) *h = (int)back_h;
}

// ---------------------------------------------------------------- SDL: audio
// The game asks for 22050 Hz mono S16 with a pull callback. We keep that
// rate (its beep queue is sized for it), pull from the callback every frame,
// and resample to the driver's 48 kHz.
static SDL_AudioCallback aud_cb;
static void* aud_user;
static int aud_rate = 22050;
static bool aud_open, aud_running;

SDL_AudioDeviceID SDL_OpenAudioDevice(const char*, int, const SDL_AudioSpec* want, SDL_AudioSpec* got, int) {
    if (!want || !want->callback) return 0;
    aud_cb = want->callback;
    aud_user = want->userdata;
    aud_rate = want->freq > 0 ? want->freq : 22050;
    if (got) { *got = *want; got->freq = aud_rate; got->format = AUDIO_S16SYS; got->channels = 1; }
    aud_open = true;
    return 1;
}
void SDL_PauseAudioDevice(SDL_AudioDeviceID, int pause_on) { aud_running = aud_open && !pause_on; }
void SDL_CloseAudioDevice(SDL_AudioDeviceID) { aud_open = aud_running = false; }

static void pump_audio(int out_frames) {
    if (out_frames <= 0) return;
    static int16_t out[4096];
    if (out_frames > 4096) out_frames = 4096;
    if (!aud_running) {                                // the card loops its buffer: keep it silent
        memset(out, 0, out_frames * sizeof(int16_t));
        audio_submit(out, out_frames);
        return;
    }
    static int16_t src[64];                            // small bites: a new tone isn't queued behind much
    static int src_len, src_idx;                       // src[src_idx] is the next unread sample
    static int16_t prev, cur;                          // interpolation endpoints
    static uint32_t frac;                              // 16.16 position between prev and cur
    const uint32_t step = (uint32_t)(((uint64_t)aud_rate << 16) / audio_rate());
    for (int i = 0; i < out_frames; i++) {
        while (frac >= 0x10000) {
            frac -= 0x10000;
            if (src_idx >= src_len) {
                aud_cb(aud_user, (Uint8*)src, (int)sizeof(src));
                src_len = 64;
                src_idx = 0;
            }
            prev = cur;
            cur = src[src_idx++];
        }
        out[i] = (int16_t)(prev + (((int32_t)(cur - prev) * (int32_t)frac) >> 16));
        frac += step;
    }
    audio_submit(out, out_frames);
}

// Feed the card for the time since the last feed, in 240 Hz steps (see
// irq.cpp), so it only needs a short cushion of queued sound.
static void pump_audio_now() {
    static uint32_t last_fine;
    uint32_t f = fine_ticks, d = f - last_fine;
    if (!d) return;
    last_fine = f;
    if (d > 24) d = 24;                                // after a long stall, don't flood
    pump_audio(audio_frames_wanted((int)(audio_rate() * d / (TICK_HZ * 4))));
}

// ---------------------------------------------------------------- quit dialog
// Same box as willy_main.cpp; "quitting" reboots the machine.
static void draw_quit_dialog() {
    const char* msg  = "QUIT WILLY THE WORM?";
    const char* hint = "Y TO REBOOT  -  N OR ESC TO CANCEL";
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

// ---------------------------------------------------------------- main
static void fatal(const char* l1, const char* l2) {
    printf("%s\n%s\n", l1, l2);
    if (back) {
        memset(back, 0, back_w * back_h * 4);
        gl_draw_text(l1, 40, 40, 1.f, 0.4f, 0.4f, 1.f, 2.f);
        gl_draw_text(l2, 40, 100, 1.f, 1.f, 1.f, 1.f, 1.f);
        present();
    }
    for (;;) __asm__ volatile("cli; hlt");
}

#ifndef __x86_64__
// Willy uses floating point everywhere (drawing included), so without an
// FPU (a 386 or 486SX on its own) say so with the 8x16 font, integers only.
extern "C" uint32_t fpu_present;                   // boot32.S
static void no_fpu_screen() {
    static const char* const lines[] = {
        "WILLY THE WORM NEEDS A MATH COPROCESSOR",
        "This PC has no 387 (or 486DX) floating-point unit.",
    };
    printf("No FPU: Willy needs a 387\n");
    memset(back, 0, (size_t)back_w * back_h * 4);
    for (int l = 0; l < 2; l++)
        for (int i = 0; lines[l][i]; i++) {
            char ch = lines[l][i] < 32 || lines[l][i] > 126 ? '?' : lines[l][i];
            const unsigned char* g = font8x16[ch - 32];
            for (int r = 0; r < 32; r++)                        // doubled: 16x32 cells
                for (int b = 0; b < 16; b++)
                    if (g[r / 2] & (0x80 >> (b / 2))) {
                        uint32_t x = 40 + i * 16 + b, y = 40 + l * 48 + r;
                        if (x < back_w && y < back_h) back[y * back_w + x] = l ? 0xFFFFFF : 0xFF6060;
                    }
        }
    present();
    for (;;) __asm__ volatile("cli; hlt");
}
#endif

extern "C" void (*__init_array_start[])(), (*__init_array_end[])();

extern "C" void kmain() {
    serial_init();
    printf("\nWilly the Worm - bare metal\n");
    for (auto f = __init_array_start; f != __init_array_end; f++) (*f)();

    const MultibootInfo* mbi = (const MultibootInfo*)(uintptr_t)mb_info;
    if (mb_magic != 0x2BADB002) { printf("Not booted by a Multiboot loader\n"); return; }
    // Keep what we need from the boot information before the heap can
    // reuse the memory it lives in.
    static MultibootInfo info;
    info = *mbi;
    static char cmdbuf[512];
    const char* cmdline = nullptr;
    if (info.flags & (1 << 2)) {
        strncpy(cmdbuf, (const char*)(uintptr_t)info.cmdline, sizeof(cmdbuf) - 1);
        cmdline = cmdbuf;
    }
    mbi = &info;
    heap_init(mbi);
    if (!video_init(mbi)) { printf("No usable framebuffer found\n"); return; }

    back_w = fb_w; back_h = fb_h;
    back = (uint32_t*)malloc((size_t)back_w * back_h * 4);
    if (!back) { printf("No memory for the back buffer\n"); return; }
    memset(back, 0, (size_t)back_w * back_h * 4);
#ifndef __x86_64__
    if (!fpu_present) no_fpu_screen();
#endif
    render_set_screen(back_w, back_h);

    audio_init(cmdline);
    usb_init(cmdline);
    bool debug = false;
    for (const char* p = cmdline; p && *p; p++)
        if (strncmp(p, "debug", 5) == 0) debug = true;

    gl_render_init(nullptr);
    static WoprState w;
    wopr_willy_enter(&w);
    if (!w.sub_state) {
        const char* why = w.lines.empty() ? "" : w.lines[0].c_str();
        fatal("Willy failed to start", why);
    }

    interrupts_init();
    storage_init(info.flags, info.boot_device, cmdline);   // after interrupts: the drive needs the timer
    printf("Running. Esc quits (reboots).\n");

    const int px = 40, py = 40;
    const int cw = gl_font_cell_size(), ch = gl_font_cell_size();
    const int cols = ((int)back_w - px * 2) / cw;

    bool quit_confirm = false;
    uint32_t last = ticks;
    for (;;) {
        // Top the sound up every 240 Hz tick while waiting for the next frame.
        while (ticks == last) { pump_audio_now(); __asm__ volatile("hlt"); }
        uint32_t now = ticks, elapsed = now - last;
        last = now;
        if (elapsed > 6) elapsed = 6;                  // clamp stalls, like the SDL build's 0.1 s
        double dt = (double)elapsed / TICK_HZ;

        floppy_poll();
        usb_poll();
        while (SDL_Keycode k = next_key()) {
            if (quit_confirm) {
                if (k == SDLK_y || k == SDLK_RETURN || k == SDLK_KP_ENTER) reboot();
                else if (k == SDLK_n || k == SDLK_ESCAPE) quit_confirm = false;
                continue;
            }
            if (k == SDLK_ESCAPE) {
                if (wopr_willy_escape_is_ingame(&w)) wopr_willy_keydown(&w, k);
                else quit_confirm = true;
                continue;
            }
            wopr_willy_keydown(&w, k);
        }

        if (!quit_confirm) wopr_willy_update(&w, dt);
        pump_audio_now();                              // sounds the update just queued

        wopr_willy_render(&w, px, py, cw, ch, cols);
        if (quit_confirm) draw_quit_dialog();
        present();

        if (debug) {
            static uint32_t frames, last_report;
            frames++;
            if (ticks - last_report >= TICK_HZ) {
                last_report = ticks;
                printf("heartbeat: ticks %u frames %u audio %s pos %u delay %u ms underruns %u heap peak %lu KB free %lu KB\n",
                       ticks, frames, audio_name(), audio_play_pos(), audio_delay_ms(), audio_underruns(),
                       (unsigned long)(heap_peak_bytes() >> 10), (unsigned long)(heap_free_bytes() >> 10));
            }
        }
    }
}

extern "C" void fault_handler() {
    printf("CPU exception - halted\n");
}
