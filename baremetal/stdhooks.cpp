// libstdc++'s out-of-line error hooks and std::random_device backend,
// normally found in libstdc++.so.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <random>

extern volatile uint32_t ticks;                  // irq.cpp

// ---------------------------------------------------------------- libstdc++ hooks
namespace std {
[[noreturn]] static void die(const char* what, const char* msg) {
    printf("%s: %s\n", what, msg ? msg : "");
    abort();
    __builtin_unreachable();
}
void __throw_logic_error(const char* m) { die("logic_error", m); }
void __throw_length_error(const char* m) { die("length_error", m); }
void __throw_out_of_range(const char* m) { die("out_of_range", m); }
void __throw_invalid_argument(const char* m) { die("invalid_argument", m); }
void __throw_bad_alloc() { die("bad_alloc", nullptr); }
void __throw_bad_array_new_length() { die("bad_array_new_length", nullptr); }
void __throw_out_of_range_fmt(const char* m, ...) { die("out_of_range", m); }
void __glibcxx_assert_fail(const char* file, int line, const char* fn, const char* cond) noexcept {
    printf("assertion failed: %s:%d %s: %s\n", file, line, fn, cond);
    abort();
}

// std::random_device: seed from the time-stamp counter, mixed with RDRAND
// when the CPU has them. A 386 or 486 has neither (nor CPUID to ask), so
// there it's the PIT's running count, the tick counter and the CMOS clock.
static uint64_t rd_state;
static void cpuid(uint32_t leaf, uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) {
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(leaf), "c"(0));
}
static uint32_t features_ecx, features_edx;      // CPUID leaf 1
static bool features_known;
static void read_features() {
    if (features_known) return;
    features_known = true;
#ifndef __x86_64__
    uint32_t before, after;                      // CPUID exists if EFLAGS.ID (bit 21) can be flipped
    __asm__ volatile("pushfl; popl %0; movl %0, %1; xorl $0x200000, %1; pushl %1; popfl; pushfl; popl %1; pushl %0; popfl"
                     : "=&r"(before), "=&r"(after));
    if (!((before ^ after) & 0x200000)) return;
#endif
    uint32_t a, b, c, d;
    cpuid(0, a, b, c, d);
    if (a < 1) return;
    cpuid(1, a, b, features_ecx, features_edx);
}
static uint64_t raw_entropy() {
    read_features();
    uint64_t r = 0;
    if (features_ecx & (1u << 30)) {             // RDRAND
        unsigned long v = 0;
        unsigned char ok;
        __asm__ volatile("rdrand %0; setc %1" : "=r"(v), "=qm"(ok));
        if (ok) r = v;
    }
    if (features_edx & (1u << 4)) {              // TSC
        uint32_t lo, hi;
        __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
        return r ^ ((uint64_t)hi << 32 | lo);
    }
    uint8_t lo, hi;
    __asm__ volatile("outb %%al, $0x43" :: "a"((uint8_t)0x00));       // latch PIT channel 0
    __asm__ volatile("inb $0x40, %%al" : "=a"(lo));
    __asm__ volatile("inb $0x40, %%al" : "=a"(hi));
    uint8_t sec;
    __asm__ volatile("outb %%al, $0x70; inb $0x71, %%al" : "=a"(sec) : "a"((uint8_t)0x00));
    return r ^ ((uint64_t)ticks << 24) ^ ((uint64_t)hi << 8 | lo) ^ ((uint64_t)sec << 48);
}
void random_device::_M_init(const std::string&) {
    rd_state ^= raw_entropy() * 0x9E3779B97F4A7C15ull;
}
void random_device::_M_fini() {}
random_device::result_type random_device::_M_getval() {
    rd_state += 0x9E3779B97F4A7C15ull ^ raw_entropy();
    uint64_t z = rd_state;                            // splitmix64
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return (result_type)(z ^ (z >> 31));
}
} // namespace std

