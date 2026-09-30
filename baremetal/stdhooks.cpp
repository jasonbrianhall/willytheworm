// libstdc++'s out-of-line error hooks and std::random_device backend,
// normally found in libstdc++.so.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <random>

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
// when the CPU has it.
static uint64_t rd_state;
static bool have_rdrand() {
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    return c & (1u << 30);
}
void random_device::_M_init(const std::string&) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    rd_state ^= ((uint64_t)hi << 32 | lo) * 0x9E3779B97F4A7C15ull;
}
void random_device::_M_fini() {}
random_device::result_type random_device::_M_getval() {
    uint64_t r = 0;
    if (have_rdrand()) { unsigned char ok; __asm__ volatile("rdrand %0; setc %1" : "=r"(r), "=qm"(ok)); if (!ok) r = 0; }
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    rd_state += 0x9E3779B97F4A7C15ull ^ r ^ ((uint64_t)hi << 32 | lo);
    uint64_t z = rd_state;                            // splitmix64
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return (result_type)(z ^ (z >> 31));
}
} // namespace std

