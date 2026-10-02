// Audio output for the bare-metal build: Intel HD Audio, Intel AC'97 and the
// Sound Blaster (SB16: 16-bit mono, 44.1 kHz; SB Pro / 2.0: 8-bit mono,
// 22 kHz). HD Audio and AC'97 play from the same looping DMA ring (16-bit
// stereo, 48 kHz); the Sound Blaster from its own, over ISA DMA. Each frame the main loop hands us the
// game's mono samples and we write them a little ahead of the hardware's
// play position. No interrupts are used.
// (Driver shared with the Super Mario Bros. bare-metal build.)
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "hw.hpp"
#include "pci.hpp"
#include "audio.hpp"

static const int kRate = 48000;

static AudioDriver driver = AUDIO_NONE;

// Shared ring: RING_BUFS chunks of CHUNK_FRAMES stereo frames.
#define RING_BUFS    32
#define CHUNK_FRAMES 512
#define RING_FRAMES  (RING_BUFS * CHUNK_FRAMES)
static int16_t ring[RING_FRAMES * 2] __attribute__((aligned(4096)));
static uint32_t write_pos;        // next frame we will write
static uint32_t target_ahead;     // desired latency in frames

static void io_delay(int n) { while (n--) inb(0x80); }   // ~1 us each

// ================================================================ AC'97
struct __attribute__((packed)) AcBDL { uint32_t addr; uint16_t samples; uint16_t flags; };
static AcBDL ac_bdl[RING_BUFS] __attribute__((aligned(8)));
static uint16_t ac_nam, ac_nabm;

static bool ac97_init() {
    PciDevice d;
    if (!pci_find_class(0x04, 0x01, -1, &d)) return false;
    ac_nam  = pci_read(d, 0x10) & 0xFFFC;
    ac_nabm = pci_read(d, 0x14) & 0xFFFC;
    if (!ac_nam || !ac_nabm) return false;
    pci_write(d, 0x04, pci_read(d, 0x04) | 0x05);     // I/O + bus master

    outl(ac_nabm + 0x2C, 0x00000002);                 // cold reset release
    io_delay(20000);
    outw(ac_nam + 0x00, 0);                           // codec reset
    io_delay(20000);
    outw(ac_nam + 0x02, 0x0000);                      // master volume: max, unmuted
    outw(ac_nam + 0x18, 0x0808);                      // PCM out volume
    if (inw(ac_nam + 0x28) & 1) {                     // variable rate: ask for 48 kHz
        outw(ac_nam + 0x2A, inw(ac_nam + 0x2A) | 1);
        outw(ac_nam + 0x2C, 48000);
    }

    for (int i = 0; i < RING_BUFS; i++) {
        ac_bdl[i].addr = (uint32_t)(uintptr_t)&ring[i * CHUNK_FRAMES * 2];
        ac_bdl[i].samples = CHUNK_FRAMES * 2;         // counted in 16-bit samples
        ac_bdl[i].flags = 0;
    }
    outb(ac_nabm + 0x1B, 0x02);                       // reset PCM-out engine
    io_delay(1000);
    outl(ac_nabm + 0x10, (uint32_t)(uintptr_t)ac_bdl);
    outb(ac_nabm + 0x15, RING_BUFS - 1);              // last valid index
    outb(ac_nabm + 0x1B, 0x01);                       // run
    return true;
}

static uint32_t ac97_play_pos() {
    uint8_t civ = inb(ac_nabm + 0x14) & 31;
    outb(ac_nabm + 0x15, (civ + RING_BUFS - 1) & 31); // keep LVI behind us so it loops forever
    outw(ac_nabm + 0x16, 0x1C);                       // clear status bits
    uint32_t left = inw(ac_nabm + 0x18) / 2;          // frames left in current buffer
    if (left > CHUNK_FRAMES) left = CHUNK_FRAMES;
    return civ * CHUNK_FRAMES + (CHUNK_FRAMES - left);
}

// ================================================================ HD Audio
// Controller: CORB/RIRB for codec verbs, one output stream whose BDL covers
// the ring. Codec: walk the audio function group, find every connected
// output pin (line out, speaker, headphone), follow its connections to a
// DAC, unmute everything on the way and point every DAC at our stream.
static volatile uint8_t* hda;
static uint32_t corb[256] __attribute__((aligned(1024)));
static uint64_t rirb[256] __attribute__((aligned(2048)));
struct __attribute__((packed)) HdaBDL { uint64_t addr; uint32_t len; uint32_t ioc; };
static HdaBDL hda_bdl[RING_BUFS] __attribute__((aligned(128)));
static uint16_t rirb_rp;
static uint32_t sd;               // output stream descriptor offset

static inline uint8_t  r8 (uint32_t o) { return *(volatile uint8_t*)(hda + o); }
static inline uint16_t r16(uint32_t o) { return *(volatile uint16_t*)(hda + o); }
static inline uint32_t r32(uint32_t o) { return *(volatile uint32_t*)(hda + o); }
static inline void w8 (uint32_t o, uint8_t v)  { *(volatile uint8_t*)(hda + o) = v; }
static inline void w16(uint32_t o, uint16_t v) { *(volatile uint16_t*)(hda + o) = v; }
static inline void w32(uint32_t o, uint32_t v) { *(volatile uint32_t*)(hda + o) = v; }

static bool wait_bits32(uint32_t off, uint32_t mask, uint32_t want, int us) {
    while (us-- > 0) { if ((r32(off) & mask) == want) return true; io_delay(1); }
    return false;
}

// Send one verb and wait for its response. Returns false on timeout.
static bool hda_cmd(uint32_t verb, uint32_t* resp) {
    uint16_t wp = (r16(0x48) + 1) & 0xFF;
    corb[wp] = verb;
    w16(0x48, wp);
    for (int i = 0; i < 20000; i++) {
        uint16_t rwp = r16(0x58) & 0xFF;
        if (rwp != rirb_rp) {
            rirb_rp = (rirb_rp + 1) & 0xFF;
            if (resp) *resp = (uint32_t)rirb[rirb_rp];
            w8(0x5D, 0x05);                           // clear RIRB status
            return true;
        }
        io_delay(1);
    }
    return false;
}
static uint32_t verb(int cad, int nid, uint32_t v, uint32_t payload) {   // 12-bit verb, 8-bit payload
    uint32_t r = 0;
    hda_cmd((uint32_t)cad << 28 | (uint32_t)nid << 20 | v << 8 | payload, &r);
    return r;
}
static uint32_t verb4(int cad, int nid, uint32_t v, uint32_t payload) {  // 4-bit verb, 16-bit payload
    uint32_t r = 0;
    hda_cmd((uint32_t)cad << 28 | (uint32_t)nid << 20 | v << 16 | payload, &r);
    return r;
}
static uint32_t param(int cad, int nid, int p) { return verb(cad, nid, 0xF00, p); }

struct Widget { uint8_t type; uint8_t nconn; uint8_t conn[16]; uint32_t caps; };
static Widget widgets[128];

static void unmute_out(int cad, int nid) {
    uint32_t caps = param(cad, nid, 0x12);            // output amp caps
    uint32_t gain = caps & 0x7F;                      // offset = 0 dB
    verb4(cad, nid, 0x3, 0xB000 | gain);              // output, L+R
}
static void unmute_in(int cad, int nid, int index) {
    uint32_t caps = param(cad, nid, 0x0D);            // input amp caps
    uint32_t gain = caps & 0x7F;
    verb4(cad, nid, 0x3, 0x7000 | (index << 8) | gain);
}

// Depth-first search from a pin to a DAC; on success configures the path.
static int route_to_dac(int cad, int nid, int depth) {
    if (depth > 6 || nid >= 128) return -1;
    Widget& w = widgets[nid];
    if (w.type == 0) return nid;                      // audio output (DAC)
    for (int i = 0; i < w.nconn; i++) {
        int dac = route_to_dac(cad, w.conn[i], depth + 1);
        if (dac < 0) continue;
        if (w.type != 2 && w.nconn > 1)
            verb(cad, nid, 0x701, i);                 // connection select (mixers sum instead)
        if (w.caps & (1 << 1)) unmute_in(cad, nid, i);
        if (w.caps & (1 << 2)) unmute_out(cad, nid);
        verb(cad, nid, 0x705, 0);                     // power D0
        return dac;
    }
    return -1;
}

static bool hda_setup_codec(int cad, uint8_t stream_tag, uint16_t fmt) {
    uint32_t sub = param(cad, 0, 0x04);
    int fg_start = (sub >> 16) & 0xFF, fg_count = sub & 0xFF;
    bool any = false;
    for (int fg = fg_start; fg < fg_start + fg_count; fg++) {
        if ((param(cad, fg, 0x05) & 0xFF) != 0x01) continue;  // audio function group only
        verb(cad, fg, 0x705, 0);                               // power up the group
        io_delay(10000);
        uint32_t ws = param(cad, fg, 0x04);
        int start = (ws >> 16) & 0xFF, count = ws & 0xFF;

        // Read every widget's type and connection list.
        for (int n = start; n < start + count && n < 128; n++) {
            Widget& w = widgets[n];
            w.caps = param(cad, n, 0x09);
            w.type = (w.caps >> 20) & 0xF;
            uint32_t cl = param(cad, n, 0x0E);
            int len = cl & 0x7F;
            bool lng = cl & 0x80;
            w.nconn = 0;
            if (!(w.caps & (1 << 8)) || lng) continue;          // no list, or long form (rare)
            for (int i = 0; i < len && w.nconn < 16; i += 4) {
                uint32_t e = verb(cad, n, 0xF02, i);
                for (int k = 0; k < 4 && i + k < len; k++)
                    w.conn[w.nconn++] = (e >> (8 * k)) & 0xFF;
            }
        }

        // Route every connected analog output pin.
        for (int n = start; n < start + count && n < 128; n++) {
            Widget& w = widgets[n];
            if (w.type != 4) continue;                               // pin complex
            uint32_t pcaps = param(cad, n, 0x0C);
            if (!(pcaps & (1 << 4))) continue;                       // not output capable
            uint32_t cfg = verb(cad, n, 0xF1C, 0);
            int conn = cfg >> 30, dev = (cfg >> 20) & 0xF;
            if (conn == 1) continue;                                 // nothing attached
            if (dev != 0x0 && dev != 0x1 && dev != 0x2) continue;    // line out, speaker, HP
            int dac = route_to_dac(cad, n, 0);
            if (dac < 0) continue;
            verb(cad, n, 0x707, dev == 0x2 ? 0xC0 : 0x40);           // out enable (+HP amp)
            if (pcaps & (1 << 16)) verb(cad, n, 0x70C, 0x02);        // EAPD on
            verb4(cad, dac, 0x2, fmt);                               // converter format
            verb(cad, dac, 0x706, stream_tag << 4);                  // stream tag, channel 0
            unmute_out(cad, dac);
            verb(cad, dac, 0x705, 0);
            printf("HDA: codec %d pin %d -> DAC %d (%s)\n", cad, n, dac,
                   dev == 0 ? "line out" : dev == 1 ? "speaker" : "headphone");
            any = true;
        }
    }
    return any;
}

static bool hda_init() {
    PciDevice d;
    if (!pci_find_class(0x04, 0x03, -1, &d)) return false;
    uint32_t bar = pci_read(d, 0x10);
    uint64_t base = bar & 0xFFFFFFF0;
    if ((bar & 0x6) == 0x4) base |= (uint64_t)pci_read(d, 0x14) << 32;
    if (base == 0 || base >= phys_limit) { printf("HDA: BAR out of reach\n"); return false; }
    hda = (volatile uint8_t*)(uintptr_t)base;
    pci_write(d, 0x04, pci_read(d, 0x04) | 0x06);    // memory + bus master

    // Controller reset.
    w32(0x08, r32(0x08) & ~1u);
    if (!wait_bits32(0x08, 1, 0, 100000)) return false;
    w32(0x08, r32(0x08) | 1);
    if (!wait_bits32(0x08, 1, 1, 100000)) return false;
    io_delay(2000);                                   // codecs report in within 521 us
    uint16_t codecs = r16(0x0E);
    if (!codecs) { printf("HDA: no codecs\n"); return false; }

    // CORB / RIRB, 256 entries each.
    w8(0x4C, 0); w8(0x5C, 0);                         // stop DMA engines
    io_delay(1000);
    w32(0x40, (uint32_t)(uintptr_t)corb); w32(0x44, 0);
    w8(0x4E, 0x02);
    w16(0x48, 0);
    w16(0x4A, 0x8000);                                // reset read pointer
    for (int i = 0; i < 1000 && !(r16(0x4A) & 0x8000); i++) io_delay(1);
    w16(0x4A, 0);
    for (int i = 0; i < 1000 && (r16(0x4A) & 0x8000); i++) io_delay(1);
    w32(0x50, (uint32_t)(uintptr_t)rirb); w32(0x54, 0);
    w8(0x5E, 0x02);
    w16(0x58, 0x8000);                                // reset write pointer
    w16(0x5A, 0xFF);                                  // response interrupt count (unused)
    rirb_rp = 0;
    w8(0x4C, 0x02);                                   // CORB run
    w8(0x5C, 0x02);                                   // RIRB run

    // First output stream: descriptors follow the input streams.
    uint16_t gcap = r16(0x00);
    int iss = (gcap >> 8) & 0xF, oss = (gcap >> 12) & 0xF;
    if (!oss) { printf("HDA: no output streams\n"); return false; }
    sd = 0x80 + iss * 0x20;
    const uint8_t tag = 1;
    const uint16_t fmt = 0x0011;                      // 48 kHz, 16-bit, 2 channels

    bool routed = false;
    for (int cad = 0; cad < 15; cad++)
        if (codecs & (1 << cad)) routed |= hda_setup_codec(cad, tag, fmt);
    if (!routed) { printf("HDA: no usable output pin\n"); return false; }

    // Stream reset, then program the BDL over the shared ring.
    w8(sd + 0, r8(sd + 0) | 1);
    for (int i = 0; i < 1000 && !(r8(sd + 0) & 1); i++) io_delay(1);
    w8(sd + 0, r8(sd + 0) & ~1);
    for (int i = 0; i < 1000 && (r8(sd + 0) & 1); i++) io_delay(1);

    for (int i = 0; i < RING_BUFS; i++) {
        hda_bdl[i].addr = (uint64_t)(uintptr_t)&ring[i * CHUNK_FRAMES * 2];
        hda_bdl[i].len = CHUNK_FRAMES * 4;
        hda_bdl[i].ioc = 0;
    }
    w32(sd + 0x18, (uint32_t)(uintptr_t)hda_bdl);
    w32(sd + 0x1C, 0);
    w32(sd + 0x08, sizeof(ring));                     // cyclic buffer length
    w16(sd + 0x0C, RING_BUFS - 1);                    // last valid index
    w16(sd + 0x12, fmt);
    w8(sd + 0x02, tag << 4);                          // stream number (CTL bits 23:20)
    w8(sd + 0x00, 0x02);                              // run
    return true;
}

static uint32_t hda_play_pos() {
    return (r32(sd + 0x04) / 4) % RING_FRAMES;       // link position in buffer
}


// ================================================================ Sound Blaster
// Two modes, picked by the DSP version:
//  - SB16 (DSP 4.xx, and the clones that speak it): 16-bit signed mono at
//    44.1 kHz, from an auto-initialising ring on a 16-bit ISA DMA channel
//    (5 by default; BLASTER's H).
//  - SB Pro 2.0 / SB 2.0 (DSP 2.01-3.xx): 8-bit unsigned mono at 22 kHz,
//    the rate set by a time constant, on an 8-bit channel (1; BLASTER's D).
// The DSP's interrupt is left masked at the PIC; the play position comes
// from the DMA controller's count, as with the other cards.
static uint16_t sb_base = 0x220;
static int sb_dma = 1;                       // 8-bit channel (0-3)
static int sb_hdma = 5;                      // 16-bit channel (5-7)
static bool sb16;                            // playing 16-bit through sb_hdma
static uint32_t sb_rate;                     // output rate in Hz
#define SB_RING 16384                        // frames: 0.37 s at 44.1 kHz, 0.74 s at 22 kHz
// One buffer for either mode: 32 KB (16-bit) or 16 KB (8-bit), 64 KB
// aligned so it never crosses an ISA DMA page (64 KB for 8-bit channels,
// 128 KB for 16-bit ones).
static uint8_t sb_buf[SB_RING * 2] __attribute__((aligned(65536)));
static uint32_t sb_write;                    // next frame we will write
static uint32_t sb_ahead;                    // desired latency in frames

static bool sb_wait_write() {
    for (int i = 0; i < 20000; i++) if (!(inb(sb_base + 0xC) & 0x80)) return true;
    return false;
}
static bool sb_wait_read() {
    for (int i = 0; i < 20000; i++) if (inb(sb_base + 0xE) & 0x80) return true;
    return false;
}
static void sb_out(uint8_t v) { if (sb_wait_write()) outb(sb_base + 0xC, v); }
static int sb_in() { return sb_wait_read() ? inb(sb_base + 0xA) : -1; }

static bool sb_reset() {
    outb(sb_base + 0x6, 1);
    io_delay(10);
    outb(sb_base + 0x6, 0);
    for (int i = 0; i < 100; i++) {
        if ((inb(sb_base + 0xE) & 0x80) && inb(sb_base + 0xA) == 0xAA) return true;
        io_delay(10);
    }
    return false;
}

static void sb_mixer(uint8_t reg, uint8_t v) { outb(sb_base + 4, reg); outb(sb_base + 5, v); }

// 8237 registers. Channels 0-3 (8-bit) live at 0x00-0x0F, 4-7 (16-bit, which
// count in words) at 0xC0-0xDF; index by channel.
static const uint8_t kDmaAddr[8]  = {0x00, 0x02, 0x04, 0x06, 0xC0, 0xC4, 0xC8, 0xCC};
static const uint8_t kDmaCount[8] = {0x01, 0x03, 0x05, 0x07, 0xC2, 0xC6, 0xCA, 0xCE};
static const uint8_t kDmaPage[8]  = {0x87, 0x83, 0x81, 0x82, 0x8F, 0x8B, 0x89, 0x8A};
static int dma_channel() { return sb16 ? sb_hdma : sb_dma; }

// Single mode, auto-init, memory to device, `units` bytes (8-bit channel)
// or words (16-bit channel) starting at `phys`.
static void dma_start(int ch, uintptr_t phys, uint32_t units) {
    bool hi = ch >= 4;
    uint16_t mask = hi ? 0xD4 : 0x0A, mode = hi ? 0xD6 : 0x0B, ff = hi ? 0xD8 : 0x0C;
    uint32_t addr = hi ? (phys >> 1) & 0xFFFF : phys & 0xFFFF;   // 16-bit channels address words
    uint8_t page = hi ? (uint8_t)((phys >> 16) & 0xFE) : (uint8_t)(phys >> 16);
    outb(mask, 0x04 | (ch & 3));                      // mask the channel
    outb(ff, 0);                                      // clear the byte flip-flop
    outb(mode, 0x58 | (ch & 3));
    outb(kDmaAddr[ch], addr & 0xFF);
    outb(kDmaAddr[ch], (addr >> 8) & 0xFF);
    outb(kDmaPage[ch], page);
    outb(kDmaCount[ch], (units - 1) & 0xFF);
    outb(kDmaCount[ch], ((units - 1) >> 8) & 0xFF);
    outb(mask, ch & 3);                               // unmask
}

static bool sb_init(const char* cmdline) {
    // sb=220,1,5  (port, 8-bit DMA channel, 16-bit DMA channel)
    for (const char* p = cmdline; p && *p; p++)
        if (!strncmp(p, "sb=", 3)) {
            unsigned v = 0; const char* q = p + 3;
            for (; (*q >= '0' && *q <= '9') || ((*q | 32) >= 'a' && (*q | 32) <= 'f'); q++)
                v = v * 16 + (*q <= '9' ? *q - '0' : (*q | 32) - 'a' + 10);
            if (v >= 0x200 && v <= 0x280) sb_base = (uint16_t)v;
            if (*q == ',' && q[1] >= '0' && q[1] <= '3') { sb_dma = q[1] - '0'; q += 2; }
            if (*q == ',' && q[1] >= '5' && q[1] <= '7') sb_hdma = q[1] - '0';
        }
    if (!sb_reset()) return false;
    sb_out(0xE1);                                     // DSP version
    int major = sb_in(), minor = sb_in();
    if (major < 2) return false;                      // the SB 1.x has no auto-init DMA
    uintptr_t phys = (uintptr_t)sb_buf;
    if (phys + sizeof sb_buf > 0x1000000) return false;   // ISA DMA reaches the first 16 MB only
    sb16 = major >= 4;

    if (sb16) {
        // SB16 mixer: master and voice full, both sides.
        sb_mixer(0x30, 0xF8); sb_mixer(0x31, 0xF8);
        sb_mixer(0x32, 0xF8); sb_mixer(0x33, 0xF8);
        memset(sb_buf, 0, sizeof sb_buf);             // signed: silence is 0
        dma_start(sb_hdma, phys, SB_RING);            // words
        sb_rate = 44100;
        sb_out(0xD1);                                 // speaker on (ignored by the SB16, needed by some clones)
        sb_out(0x41); sb_out(sb_rate >> 8); sb_out(sb_rate & 0xFF);   // output rate
        sb_out(0xB6);                                 // 16-bit, D/A, auto-init, FIFO on
        sb_out(0x10);                                 // mono, signed
        sb_out((SB_RING - 1) & 0xFF); sb_out(((SB_RING - 1) >> 8) & 0xFF);   // block, in samples
        printf("Audio: Sound Blaster 16 (DSP %d.%02d) at %Xh, DMA %d\n",
               major, minor < 0 ? 0 : minor, sb_base, sb_hdma);
        return true;
    }

    // Mixer (SB Pro and later): mono output, filter on, voice and master full.
    if (major >= 3) { sb_mixer(0x0E, 0x00); sb_mixer(0x04, 0xFF); sb_mixer(0x22, 0xFF); }
    memset(sb_buf, 0x80, sizeof sb_buf);              // unsigned: silence is 128
    dma_start(sb_dma, phys, SB_RING);                 // bytes

    // DSP: speaker on, 22 kHz (time constant 256 - 1000000/rate), a block
    // the length of the ring, 8-bit auto-init output.
    int tc = 211;
    sb_rate = 1000000 / (256 - tc);                   // 22222 Hz
    sb_out(0xD1);
    sb_out(0x40); sb_out((uint8_t)tc);
    sb_out(0x48); sb_out((SB_RING - 1) & 0xFF); sb_out(((SB_RING - 1) >> 8) & 0xFF);
    sb_out(0x1C);
    printf("Audio: Sound Blaster (DSP %d.%02d) at %Xh, DMA %d\n", major, minor < 0 ? 0 : minor, sb_base, sb_dma);
    return true;
}

// Frames the DMA controller has played into the ring.
static uint32_t sb_play_pos() {
    int ch = dma_channel();
    uint16_t ff = ch >= 4 ? 0xD8 : 0x0C;
    inb(sb_base + (sb16 ? 0xF : 0xE));                // acknowledge the DSP's block interrupt
    uint32_t c1, c2;
    do {
        outb(ff, 0);
        c1 = inb(kDmaCount[ch]); c1 |= inb(kDmaCount[ch]) << 8;
        outb(ff, 0);
        c2 = inb(kDmaCount[ch]); c2 |= inb(kDmaCount[ch]) << 8;
    } while (c1 - c2 > 4 && c2 - c1 > 4);             // read twice: the count was changing
    uint32_t left = (c2 + 1) & 0xFFFF;                // units (= frames) left in this pass
    if (left > SB_RING) left = SB_RING;
    return (SB_RING - left) % SB_RING;
}

// As audio_frames_wanted, in the Sound Blaster's frames; `nominal` and the
// answer are 48 kHz frames. The DMA count moves in bursts (QEMU's SB16 in
// 1-5 ms steps, real ISA DMA in FIFO-sized ones), so the play position is
// jumpy; chasing it at full strength warbles the pitch. So it's steered
// gently: at most 0.5% per batch.
static int sb_frames_wanted(int nominal, uint32_t* underruns) {
    uint32_t play = sb_play_pos();
    uint32_t ahead = (sb_write + SB_RING - play) % SB_RING;
    int nom = (int)((uint32_t)nominal * sb_rate / 48000);
    if (ahead < sb_ahead / 8 || ahead > sb_ahead * 2) {
        if (ahead < sb_ahead / 8 || ahead >= SB_RING / 2) (*underruns)++;
        sb_write = (play + sb_ahead + SB_RING - nom % SB_RING) % SB_RING;
        return nominal;
    }
    int err = (int)(ahead + nom) - (int)sb_ahead;
    int m = nom - err / 64, lim = nom / 200 + 1;
    m = m < nom - lim ? nom - lim : m > nom + lim ? nom + lim : m;
    return (int)((uint32_t)m * 48000 / sb_rate);
}

// One output frame into the Sound Blaster's ring, in the mode it plays.
static inline void sb_put(int32_t v) {
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    if (sb16) ((int16_t*)sb_buf)[sb_write] = (int16_t)v;
    else sb_buf[sb_write] = (uint8_t)((v >> 8) + 128);
    sb_write = (sb_write + 1) % SB_RING;
}

// 48 kHz signed 16-bit in, sb_rate out. SB16 (44.1 kHz): linear
// interpolation. SB Pro (22 kHz): each output sample is the average of the
// input samples that fall in it (a box filter, so the highs don't alias).
static uint32_t sb_phase;                    // 48 kHz -> sb_rate resampling
static int32_t  sb_prev, sb_acc, sb_accn;
static void sb_submit(const int16_t* samples, int n) {
    for (int i = 0; i < n; i++) {
        int32_t x = samples[i];
        sb_phase += sb_rate;
        if (sb16) {
            if (sb_phase >= 48000) {
                sb_phase -= 48000;
                // The output instant lies sb_phase/sb_rate of a sample before x.
                int32_t f = (int32_t)(((uint64_t)sb_phase << 16) / sb_rate);
                sb_put(x - (int32_t)(((int64_t)(x - sb_prev) * f) >> 16));
            }
            sb_prev = x;
        } else {
            sb_acc += x; sb_accn++;
            if (sb_phase >= 48000) {
                sb_phase -= 48000;
                sb_put(sb_acc / sb_accn);
                sb_acc = 0; sb_accn = 0;
            }
        }
    }
}

// ================================================================ common
static bool arg_is(const char* cmdline, const char* want) {
    if (!cmdline) return false;
    for (const char* p = cmdline; *p; p++)
        if (strncmp(p, "audio=", 6) == 0 && strncmp(p + 6, want, strlen(want)) == 0) return true;
    return false;
}

AudioDriver audio_init(const char* cmdline) {
    bool want_off = arg_is(cmdline, "off");
    bool want_hda = arg_is(cmdline, "hda");
    bool want_ac  = arg_is(cmdline, "ac97");
    bool want_sb  = arg_is(cmdline, "sb");
    bool any = !want_hda && !want_ac && !want_sb;
    memset(ring, 0, sizeof(ring));
    if (!want_off) {
        if ((any || want_hda) && hda_init()) driver = AUDIO_HDA;
        else if ((any || want_ac) && ac97_init()) driver = AUDIO_AC97;
        else if ((any || want_sb) && sb_init(cmdline)) driver = AUDIO_SB;
    }
    if (driver == AUDIO_NONE) { printf("Audio: none\n"); return driver; }

    // Sound is topped up at 240 Hz (kernel.cpp), so a short cushion is enough:
    // 20 ms. A 386 can spend longer than that on one frame, so it gets 50 ms.
#ifdef __x86_64__
    int ms = 20;
#else
    int ms = 50;
#endif
    // Boot option latency=N (milliseconds, 5..200) overrides it: lower if
    // sound feels late, higher if it crackles (the debug heartbeat counts underruns).
    for (const char* p = cmdline; p && *p; p++)
        if (!strncmp(p, "latency=", 8)) {
            int v = 0;
            for (const char* q = p + 8; *q >= '0' && *q <= '9'; q++) v = v * 10 + (*q - '0');
            if (v >= 5 && v <= 200) ms = v;
        }
    if (driver == AUDIO_SB) {
        // ISA DMA moves in bursts, so the Sound Blaster needs a deeper
        // cushion than the PCI cards: at least 40 ms.
        if (ms < 40) ms = 40;
        sb_ahead = sb_rate * ms / 1000;
        sb_write = (sb_play_pos() + sb_ahead) % SB_RING;
        printf("Audio: %s at %u Hz, %d ms latency\n", audio_name(), (unsigned)sb_rate, ms);
        return driver;
    }
    target_ahead = (uint32_t)(kRate * ms / 1000);
    write_pos = (audio_play_pos() + target_ahead) % RING_FRAMES;
    printf("Audio: %s at %d Hz, %d ms latency\n", audio_name(), kRate, ms);
    return driver;
}

uint32_t audio_play_pos() {
    switch (driver) {
    case AUDIO_HDA:  return hda_play_pos();
    case AUDIO_AC97: return ac97_play_pos();
    case AUDIO_SB:   return sb_play_pos();
    default:         return 0;
    }
}

// How many frames to write now, for `nominal` frames' worth of game time,
// so the write position stays a steady target_ahead in front of the card.
// The game runs on the PIT's 60 Hz and the card plays on its own crystal;
// left alone, the gap drifts (and with it the delay you hear). So each batch
// is stretched or squeezed by up to ~3% to pull the gap back, and only a
// real stall forces a jump.
static uint32_t underruns;
uint32_t audio_underruns() { return underruns; }
int audio_frames_wanted(int nominal) {
    if (driver == AUDIO_NONE || nominal <= 0) return nominal;
    if (driver == AUDIO_SB) return sb_frames_wanted(nominal, &underruns);
    uint32_t play = audio_play_pos();
    uint32_t ahead = (write_pos + RING_FRAMES - play) % RING_FRAMES;
    if (ahead < target_ahead / 8 || ahead > target_ahead * 2) {
        if (ahead < target_ahead / 8 || ahead >= RING_FRAMES / 2) underruns++;   // ran dry: a gap you can hear
        write_pos = (play + target_ahead + RING_FRAMES - nominal % RING_FRAMES) % RING_FRAMES;   // restart the gap
        return nominal;
    }
    int err = (int)(ahead + nominal) - (int)target_ahead;      // + : more delay than wanted
    int m = nominal - err / 8, lim = nominal / 32 + 1;
    return m < nominal - lim ? nominal - lim : m > nominal + lim ? nominal + lim : m;
}

void audio_submit(const int16_t* samples, int n) {
    if (driver == AUDIO_NONE) return;
    if (driver == AUDIO_SB) { sb_submit(samples, n); return; }
    for (int i = 0; i < n; i++) {
        ring[write_pos * 2] = samples[i];
        ring[write_pos * 2 + 1] = samples[i];
        write_pos = (write_pos + 1) % RING_FRAMES;
    }
}

uint32_t audio_delay_ms() {
    if (driver == AUDIO_NONE) return 0;
    if (driver == AUDIO_SB) return (sb_write + SB_RING - sb_play_pos()) % SB_RING * 1000 / sb_rate;
    return (write_pos + RING_FRAMES - audio_play_pos()) % RING_FRAMES * 1000 / kRate;
}

const char* audio_name() {
    return driver == AUDIO_HDA ? "HD Audio" : driver == AUDIO_AC97 ? "AC97" :
           driver == AUDIO_SB ? "Sound Blaster" : "none";
}
