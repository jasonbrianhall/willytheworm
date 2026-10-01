#pragma once
// Files on the boot floppy. When Willy was booted from a FAT12 floppy
// (drive A:), the stream shim (overrides/stream_shim.h) reads and writes
// real files on it; the high-score table lands in /willy/scores.txt.
// Otherwise every file open fails, as before. Boot option floppy=off disables it.
#include <stdint.h>

void storage_init(uint32_t mb_flags, uint32_t boot_device, const char* cmdline);
