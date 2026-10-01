// Interrupt handlers. Built with -mgeneral-regs-only so they never touch
// SSE registers (the assembly stubs only save general-purpose registers).
#include <stdint.h>
#include "hw.hpp"

// The PIT runs at 240 Hz: `fine_ticks` counts every interrupt (sound is
// topped up that often), `ticks` every 4th (60 Hz, the game's frame clock).
volatile uint32_t ticks, fine_ticks;
volatile uint8_t kbd_buf[256];
volatile uint8_t kbd_head, kbd_tail;

extern "C" void irq_timer() {
    fine_ticks = fine_ticks + 1;
    if ((fine_ticks & 3) == 0) ticks = ticks + 1;
    outb(0x20, 0x20);
}

extern "C" void irq_keyboard() {
    uint8_t b = inb(0x60);
    kbd_buf[kbd_head] = b;
    kbd_head = kbd_head + 1;
    outb(0x20, 0x20);
}

extern "C" void irq_spurious() {}
