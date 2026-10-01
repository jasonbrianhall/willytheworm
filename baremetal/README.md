# Bare-metal Willy the Worm

Boots straight into the game on x86_64 PCs, or a 386 or later with a 387 (`ARCH=i386`): no OS, no libc, no SDL. GRUB,
QEMU's `-kernel` or a UEFI loader starts a small kernel that runs the
unchanged `../cpp/wopr_willy.cpp` on the framebuffer. Levels and sprites are
compiled in, so there is nothing else to load.

```
sudo apt install build-essential qemu-system-x86 grub-pc-bin grub-common xorriso mtools gnu-efi ovmf
# Fedora: gcc-c++ qemu-system-x86 grub2-tools grub2-tools-extra grub2-pc-modules xorriso mtools gnu-efi-devel edk2-ovmf
make run          # QEMU, direct kernel boot
make iso          # willy.iso: bootable CD / USB stick (dd it)
make floppy       # willy-floppy.img: 1.44 MB FAT12 boot floppy (needs mtools, dosfstools)
make efi          # willy.efi: UEFI application (make run-efi tests it under OVMF)
```

`make ARCH=i386 floppy` (or `iso`, `run`) builds the 32-bit kernel, named
`willy-i386.*`, next to the 64-bit one. It runs on a 386 or later but needs a
387 or 486DX, as the game uses floating point throughout (without one it
says so). It asks for 1024x768 at 8 bits per pixel, the mode 1 MB VESA cards
of the time offer, and draws at whatever depth it gets (8 to 32 bits); the
card needs VBE 2.0 with a linear framebuffer (UniVBE adds that to older
ones). USB is left out; about 8 MB of RAM. Needs g++-multilib.
`tools/rbtree_test.sh` checks `rbtree.cpp`, the kernel's own `std::map`
balancing (libstdc++'s 32-bit `tree.o` uses Pentium Pro instructions).

Sound runs about 40 ms ahead of the speaker, held steady: the game's 60 Hz
timer and the sound card's clock drift apart, so each frame's audio is
stretched or squeezed slightly to keep the gap fixed. With the `debug` boot
option the once-a-second heartbeat on the serial port shows it.

To boot `willy.efi` from Fedora's GRUB, copy it to `/boot/efi/EFI/willy/` and
add to `/etc/grub.d/40_custom`, then run `grub2-mkconfig`:

```
menuentry "Willy the Worm (bare metal)" {
    search --no-floppy --set=root --file /EFI/willy/willy.efi
    chainloader /EFI/willy/willy.efi
}
```

Secure Boot needs it signed (same steps as the Super Mario Bros. bare-metal build).

**Sound:** Intel HD Audio if present, otherwise AC97 (`make run SOUND=ac97`).
Boot options: `audio=hda|ac97|off`, `usb=off`, `debug` (serial heartbeat).

**Memory:** 16 MB of RAM is enough; the heap uses whatever RAM the machine has.

**Keyboard:** PS/2, or USB keyboards on an xHCI controller. Esc asks to quit,
and quitting reboots. No mouse. High scores are kept on the boot floppy (`/willy/scores.txt`) when Willy boots from it; otherwise, or with the boot option `floppy=off`, they last until reboot.

**How it builds:** the game is compiled against the normal libstdc++/glibc
headers. `overrides/` stands in for SDL2 and the file streams, `render.cpp`
draws the game's rectangles and text (DejaVu Sans Mono pre-rasterized at the
desktop size by `tools/gen_font.py`), and `runtime.cpp`/`stdhooks.cpp` supply
the handful of library symbols the headers need. The boot code and the
HDA/AC97, PCI and xHCI drivers come from the Super Mario Bros. bare-metal build.
