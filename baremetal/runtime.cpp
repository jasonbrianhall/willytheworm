// Minimal C/C++ runtime for the bare-metal build.
//
// The game is compiled against the normal libstdc++/glibc *headers*, but
// nothing from libc or libstdc++.so is linked. This file supplies the handful
// of out-of-line symbols those headers end up referencing: memory and string
// functions, snprintf/printf (printf goes to the serial port), strtol, a heap,
// and libstdc++'s error hooks and std::random_device backend.
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "hw.hpp"
// No libc headers here on purpose: their C++ overloads (memchr and friends)
// would clash with the plain C definitions below.
extern "C" size_t strlen(const char*);

extern "C" {

// ---------------------------------------------------------------- memory / strings
void* memset(void* d, int c, size_t n) {
    void* r = d;
    __asm__ volatile("rep stosb" : "+D"(d), "+c"(n) : "a"(c) : "memory");
    return r;
}
void* memcpy(void* __restrict d, const void* __restrict s, size_t n) {
    void* r = d;
    __asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(n) :: "memory");
    return r;
}
void* memmove(void* d, const void* s, size_t n) {
    uint8_t* dp = (uint8_t*)d; const uint8_t* sp = (const uint8_t*)s;
    if (dp < sp) while (n--) *dp++ = *sp++;
    else { dp += n; sp += n; while (n--) *--dp = *--sp; }
    return d;
}
int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* x = (const uint8_t*)a; const uint8_t* y = (const uint8_t*)b;
    for (; n; n--, x++, y++) if (*x != *y) return *x - *y;
    return 0;
}
void* memchr(const void* s, int c, size_t n) {
    const uint8_t* p = (const uint8_t*)s;
    for (; n; n--, p++) if (*p == (uint8_t)c) return (void*)p;
    return nullptr;
}
size_t strlen(const char* s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}
int strncmp(const char* a, const char* b, size_t n) {
    for (; n; n--, a++, b++) { if (*a != *b || !*a) return (uint8_t)*a - (uint8_t)*b; }
    return 0;
}
char* strcpy(char* d, const char* s) { char* r = d; while ((*d++ = *s++)) {} return r; }
char* strncpy(char* d, const char* s, size_t n) {
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}

// ---------------------------------------------------------------- errno / strtol
static int errno_value;
int* __errno_location(void) { return &errno_value; }

long strtol(const char* s, char** end, int base) {
    const char* p = s;
    while (*p == ' ' || (*p >= '\t' && *p <= '\r')) p++;
    bool neg = false;
    if (*p == '+' || *p == '-') neg = *p++ == '-';
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] | 32) == 'x') { p += 2; base = 16; }
    else if (base == 0) base = p[0] == '0' ? 8 : 10;
    unsigned long v = 0;
    const char* start = p;
    for (;; p++) {
        int d;
        if (*p >= '0' && *p <= '9') d = *p - '0';
        else if ((*p | 32) >= 'a' && (*p | 32) <= 'z') d = (*p | 32) - 'a' + 10;
        else break;
        if (d >= base) break;
        v = v * base + d;
    }
    if (end) *end = (char*)(p == start ? s : p);
    return neg ? -(long)v : (long)v;
}
long __isoc23_strtol(const char* s, char** end, int base) { return strtol(s, end, base); }
int atoi(const char* s) { return (int)strtol(s, nullptr, 10); }

// ---------------------------------------------------------------- formatted output
struct Out { char* buf; size_t cap, len; bool serial; };
static void emit(Out& o, char c) {
    if (o.serial) serial_putc(c);
    else if (o.len + 1 < o.cap) o.buf[o.len] = c;
    o.len++;
}
static void emit_num(Out& o, unsigned long long v, int base, bool neg, int width, char pad, bool left) {
    char tmp[32]; int i = 0;
    do { tmp[i++] = "0123456789abcdef"[v % base]; v /= base; } while (v);
    if (neg && pad == '0') { emit(o, '-'); width--; neg = false; }
    if (neg) tmp[i++] = '-';
    int fill = width > i ? width - i : 0;
    if (!left) while (fill--) emit(o, pad);
    while (i) emit(o, tmp[--i]);
    if (left) while (fill-- > 0) emit(o, ' ');
}
static void format(Out& o, const char* f, va_list ap) {
    for (; *f; f++) {
        if (*f != '%') { emit(o, *f); continue; }
        f++;
        bool left = false; char pad = ' ';
        for (;; f++) {
            if (*f == '-') left = true;
            else if (*f == '0') pad = '0';
            else if (*f == ' ' || *f == '+') {}
            else break;
        }
        int width = 0, prec = -1;
        if (*f == '*') { width = va_arg(ap, int); f++; }
        while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
        if (*f == '.') { f++; prec = 0; while (*f >= '0' && *f <= '9') prec = prec * 10 + (*f++ - '0'); }
        int lng = 0;
        while (*f == 'l' || *f == 'z' || *f == 'h') { if (*f != 'h') lng++; f++; }
        switch (*f) {
        case 'd': case 'i': {
            long long v = lng ? va_arg(ap, long) : va_arg(ap, int);
            emit_num(o, v < 0 ? -(unsigned long long)v : v, 10, v < 0, width, pad, left); break; }
        case 'u': emit_num(o, lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 10, false, width, pad, left); break;
        case 'x': case 'X': emit_num(o, lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 16, false, width, pad, left); break;
        case 'p': emit(o, '0'); emit(o, 'x'); emit_num(o, (uintptr_t)va_arg(ap, void*), 16, false, 0, ' ', false); break;
        case 's': {
            const char* s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            int n = (int)strlen(s);
            if (prec >= 0 && n > prec) n = prec;
            if (!left) for (int i = n; i < width; i++) emit(o, ' ');
            for (int i = 0; i < n; i++) emit(o, s[i]);
            if (left) for (int i = n; i < width; i++) emit(o, ' ');
            break; }
        case 'c': emit(o, (char)va_arg(ap, int)); break;
        case 'f': case 'g': {
            double v = va_arg(ap, double);
            if (v < 0) { emit(o, '-'); v = -v; }
            if (prec < 0) prec = 2;
            unsigned long long ip = (unsigned long long)v;
            emit_num(o, ip, 10, false, 0, ' ', false);
            if (prec) {
                emit(o, '.');
                double fr = v - (double)ip;
                for (int i = 0; i < prec; i++) { fr *= 10; int d = (int)fr; emit(o, (char)('0' + d)); fr -= d; }
            }
            break; }
        case '%': emit(o, '%'); break;
        default: emit(o, '%'); emit(o, *f); break;
        }
    }
}
int vsnprintf(char* buf, size_t cap, const char* f, va_list ap) {
    Out o{buf, cap, 0, false};
    format(o, f, ap);
    if (cap) buf[o.len < cap ? o.len : cap - 1] = 0;
    return (int)o.len;
}
int snprintf(char* buf, size_t cap, const char* f, ...) {
    va_list ap; va_start(ap, f); int n = vsnprintf(buf, cap, f, ap); va_end(ap); return n;
}
int sprintf(char* buf, const char* f, ...) {
    va_list ap; va_start(ap, f); int n = vsnprintf(buf, (size_t)-1 >> 1, f, ap); va_end(ap); return n;
}
int vprintf(const char* f, va_list ap) { Out o{nullptr, 0, 0, true}; format(o, f, ap); return (int)o.len; }
int printf(const char* f, ...) { va_list ap; va_start(ap, f); int n = vprintf(f, ap); va_end(ap); return n; }
int puts(const char* s) { serial_puts(s); serial_putc('\n'); return 0; }
int putchar(int c) { serial_putc((char)c); return c; }

// ---------------------------------------------------------------- heap
// First-fit free list over a static arena. The level packs are held as maps
// of std::string grids, so give it plenty of room.
struct Block { size_t size; Block* next; size_t free; size_t pad; };
static uint8_t heap_area[64 << 20] __attribute__((aligned(16)));
static Block* heap_head;
static Block* heap_rover;

void* malloc(size_t n) {
    n = (n + 15) & ~(size_t)15;
    if (!n) n = 16;
    if (!heap_head) {
        heap_head = heap_rover = (Block*)heap_area;
        heap_head->size = sizeof(heap_area) - sizeof(Block);
        heap_head->next = nullptr;
        heap_head->free = 1;
    }
    for (int pass = 0; pass < 2; pass++) {
        for (Block* b = pass ? heap_head : heap_rover; b; b = b->next) {
            if (!b->free) continue;
            while (b->next && b->next->free) {             // coalesce lazily
                if (heap_rover == b->next) heap_rover = b;
                b->size += sizeof(Block) + b->next->size;
                b->next = b->next->next;
            }
            if (b->size < n) continue;
            if (b->size >= n + sizeof(Block) + 64) {
                Block* rest = (Block*)((uint8_t*)(b + 1) + n);
                rest->size = b->size - n - sizeof(Block);
                rest->next = b->next;
                rest->free = 1;
                b->next = rest;
                b->size = n;
            }
            b->free = 0;
            heap_rover = b;
            return b + 1;
        }
    }
    printf("malloc: out of memory (%lu bytes)\n", (unsigned long)n);
    return nullptr;
}
void free(void* p) {
    if (!p) return;
    Block* b = (Block*)p - 1;
    b->free = 1;
    while (b->next && b->next->free) {
        if (heap_rover == b->next) heap_rover = b;
        b->size += sizeof(Block) + b->next->size;
        b->next = b->next->next;
    }
}
void* calloc(size_t a, size_t b) { void* p = malloc(a * b); if (p) memset(p, 0, a * b); return p; }
void* realloc(void* p, size_t n) {
    if (!p) return malloc(n);
    Block* b = (Block*)p - 1;
    if (b->size >= n) return p;
    void* q = malloc(n);
    if (q) { memcpy(q, p, b->size); free(p); }
    return q;
}

void abort(void) {
    printf("abort()\n");
    for (;;) __asm__ volatile("cli; hlt");
}

// ---------------------------------------------------------------- C++ ABI
// Nothing ever throws (the only try/catch guards std::stoi on well-formed
// level data), so the exception entry points just stop the machine.
void* __dso_handle = nullptr;
int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }
void __cxa_pure_virtual() { printf("pure virtual call\n"); abort(); }
void* __cxa_begin_catch(void*) { return nullptr; }
void __cxa_end_catch() {}
void __cxa_rethrow() { printf("exception rethrown\n"); abort(); }
void _Unwind_Resume(void*) { printf("_Unwind_Resume\n"); abort(); }
int __gxx_personality_v0() { return 0; }

} // extern "C"

void* operator new(size_t n) { return malloc(n); }
void* operator new[](size_t n) { return malloc(n); }
void operator delete(void* p) noexcept { free(p); }
void operator delete[](void* p) noexcept { free(p); }
void operator delete(void* p, size_t) noexcept { free(p); }
void operator delete[](void* p, size_t) noexcept { free(p); }

// No environment: highscores.cpp falls back to "." (and never touches disk).
extern "C" char* getenv(const char*) { return nullptr; }
extern "C" int mkdir(const char*, unsigned) { return -1; }
