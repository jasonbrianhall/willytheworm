// Files on the boot floppy. See storage.hpp.
#include "storage.hpp"
#include "fat12.hpp"
#include "floppy.hpp"
#include "hw.hpp"
#include <stdio.h>
#include <string.h>
#include <string>

static bool enabled, mounted;

static void idle() { floppy_idle(); }
static const FatDisk disk = {floppy_read, floppy_write, idle};

// File dates from the CMOS clock (whatever zone the machine keeps).
static uint8_t cmos(uint8_t r) { outb(0x70, r); return inb(0x71); }
long fat_clock() {
    uint8_t s, m, h, d, mo, y, b;
    do {                                                 // read until two reads agree
        while (cmos(0x0A) & 0x80) {}
        s = cmos(0); m = cmos(2); h = cmos(4); d = cmos(7); mo = cmos(8); y = cmos(9);
    } while (s != cmos(0));
    b = cmos(0x0B);
    auto bin = [&](uint8_t v) { return (b & 4) ? v : (uint8_t)((v & 15) + (v >> 4) * 10); };
    bool pm = !(b & 2) && (h & 0x80);
    h = bin(h & 0x7F);
    if (!(b & 2)) h = (uint8_t)(h % 12 + (pm ? 12 : 0));
    long Y = 2000 + bin(y), M = bin(mo), D = bin(d);
    Y -= M <= 2;                                         // days_from_civil (H. Hinnant)
    long era = (Y >= 0 ? Y : Y - 399) / 400, yoe = Y - era * 400;
    long doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
    long days = era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
    return days * 86400 + h * 3600L + bin(m) * 60L + bin(s);
}

// The disk can be taken out or swapped while the game runs: check before
// each use, and remount when a different disk is in.
static bool ready() {
    if (!enabled) return false;
    int m = floppy_check_media();
    if (m < 0) {
        if (mounted) printf("Storage: floppy removed\n");
        mounted = false;
    } else if (m == 1 || !mounted) {
        mounted = fat_mount(disk);
        if (mounted) printf("Storage: FAT12 floppy mounted, %u KB free\n", fat_free_bytes() / 1024);
        else printf("Storage: the floppy isn't FAT12\n");
    }
    if (!mounted) floppy_idle();
    return mounted;
}

void storage_init(uint32_t mb_flags, uint32_t boot_device, const char* cmdline) {
    if (cmdline && strstr(cmdline, "floppy=off")) return;
    // Multiboot boot_device: BIOS drive number in the top byte; 0x00 is A:.
    if (!(mb_flags & (1 << 1)) || (boot_device >> 24) != 0x00) return;
    if (!floppy_init()) return;
    enabled = true;
    ready();
}

// ---- hooks for overrides/stream_shim.h
bool platform_file_load(const char* path, std::string& out) {
    bool dir;
    uint32_t size;
    if (!ready() || !fat_exists(path, &dir, &size) || dir) return false;
    out.assign(size, '\0');
    size_t got;
    return fat_read(path, &out[0], size, &got) && got == size;
}

bool platform_file_store(const char* path, const std::string& data) {
    if (!ready()) return false;
    if (const char* slash = strrchr(path, '/'); slash && slash != path)
        fat_mkdir(std::string(path, slash - path).c_str());
    if (fat_write(path, data.data(), data.size())) return true;
    printf("Storage: couldn't save %s%s\n", path, floppy_write_protected() ? " (write-protected)" : "");
    return false;
}
