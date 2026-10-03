#include "compiler_compat.h"
#include <stdarg.h>

jmp_buf comp_err_jmp;
int comp_has_error = 0;

// 1. Strings
int comp_strcmp(const char* s1, const char* s2) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

size_t comp_strlen(const char* s) {
    size_t len = 0;
    while (s && s[len]) len++;
    return len;
}

char* comp_strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* comp_strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

int comp_atoi(const char* s) {
    int res = 0;
    int sign = 1;
    while (s && (*s == ' ' || *s == '\t')) s++;
    if (s && *s == '-') { sign = -1; s++; }
    else if (s && *s == '+') s++;
    while (s && *s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res * sign;
}

char* comp_strdup(const char* s) {
    if (!s) return 0;
    size_t len = comp_strlen(s);
    char* copy = (char*)comp_calloc(1, len + 1);
    if (copy) memcpy(copy, s, len + 1);
    return copy;
}

int comp_sprintf(char* out, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* p = out;

    for (const char* f = fmt; *f; f++) {
        if (*f != '%') {
            *p++ = *f;
            continue;
        }
        f++;
        if (*f == 's') {
            const char* s = va_arg(args, const char*);
            if (!s) s = "(null)";
            while (*s) *p++ = *s++;
        } else if (*f == 'd') {
            int val = va_arg(args, int);
            if (val < 0) {
                *p++ = '-';
                val = -val;
            }
            char buf[32];
            int bi = 0;
            if (val == 0) buf[bi++] = '0';
            while (val > 0) {
                buf[bi++] = '0' + (val % 10);
                val /= 10;
            }
            while (bi > 0) *p++ = buf[--bi];
        } else if (*f == 'l' && *(f + 1) == 'd') {
            f++;
            long val = va_arg(args, long);
            if (val < 0) {
                *p++ = '-';
                val = -val;
            }
            char buf[32];
            int bi = 0;
            if (val == 0) buf[bi++] = '0';
            while (val > 0) {
                buf[bi++] = '0' + (val % 10);
                val /= 10;
            }
            while (bi > 0) *p++ = buf[--bi];
        } else if (*f == 'c') {
            char c = (char)va_arg(args, int);
            *p++ = c;
        } else if (*f == '%') {
            *p++ = '%';
        }
    }
    *p = '\0';
    va_end(args);
    return (int)(p - out);
}

// 2. Memory Arena (512 KB)
#define COMP_ARENA_SIZE (512 * 1024)
static char comp_arena[COMP_ARENA_SIZE];
static size_t comp_arena_offset = 0;

void comp_reset_arena() {
    comp_arena_offset = 0;
}

int comp_is_in_arena(void* ptr) {
    return ((char*)ptr >= comp_arena && (char*)ptr < comp_arena + COMP_ARENA_SIZE);
}

void* comp_calloc(size_t num, size_t size) {
    size_t total = num * size;
    total = (total + 7) & ~7; // 8-byte align
    if (comp_arena_offset + total > COMP_ARENA_SIZE) {
        kprintf("[COMPILER] Error: Compiler memory arena exhausted!\n");
        return 0;
    }
    void* ptr = (void*)&comp_arena[comp_arena_offset];
    comp_arena_offset += total;
    memset(ptr, 0, total);
    return ptr;
}

void* comp_realloc(void* ptr, size_t new_size) {
    void* new_ptr = comp_calloc(1, new_size);
    if (!new_ptr) return 0;
    if (ptr) {
        // Safe conservative copy
        memcpy(new_ptr, ptr, new_size);
    }
    return new_ptr;
}

void comp_free(void* ptr) {
    (void)ptr; // Arena reclaims everything on comp_reset_arena()
}

// 3. Jump mechanisms for safe error aborts
__attribute__((naked)) int comp_setjmp(jmp_buf env) {
    asm volatile (
        "mov (%rsp), %rax\n"
        "mov %rax, (%rdi)\n"
        "mov %rbx, 8(%rdi)\n"
        "mov %rsp, 16(%rdi)\n"
        "mov %rbp, 24(%rdi)\n"
        "mov %r12, 32(%rdi)\n"
        "mov %r13, 40(%rdi)\n"
        "mov %r14, 48(%rdi)\n"
        "mov %r15, 56(%rdi)\n"
        "xor %eax, %eax\n"
        "ret\n"
    );
}

__attribute__((naked)) void comp_longjmp(jmp_buf env, int val) {
    asm volatile (
        "mov %esi, %eax\n"
        "test %eax, %eax\n"
        "jnz 1f\n"
        "inc %eax\n"
        "1:\n"
        "mov (%rdi), %rdx\n"
        "mov 8(%rdi), %rbx\n"
        "mov 16(%rdi), %rsp\n"
        "mov 24(%rdi), %rbp\n"
        "mov 32(%rdi), %r12\n"
        "mov 40(%rdi), %r13\n"
        "mov 48(%rdi), %r14\n"
        "mov 56(%rdi), %r15\n"
        "mov %rdx, (%rsp)\n"
        "ret\n"
    );
}

// 4. Console Filter and Builtins
int comp_printf(const char* fmt, ...) {
    if (!fmt) return 0;
    // Suppress debug token parsing spam
    if (fmt[0] == 'A' && fmt[1] == 'd' && fmt[2] == 'v') return 0;
    if (fmt[0] == 'c' && fmt[1] == 'o' && fmt[2] == 'n') return 0;
    if (fmt[0] == 'G' && fmt[1] == 'o' && fmt[2] == 't') return 0;
    if (fmt[0] == 'i' && fmt[1] == 'n' && fmt[2] == ' ') return 0;
    if (fmt[0] == 'E' && fmt[1] == 'n' && fmt[2] == 't') return 0;
    if (fmt[0] == ' ' && fmt[1] == ' ') return 0;
    if (fmt[0] == 'D' && fmt[1] == 'E' && fmt[2] == 'B') return 0;
    if (fmt[0] == '[' && (fmt[1] == 'F' || fmt[1] == 'I' || fmt[1] == 'G' || fmt[1] == 'V' || fmt[1] == 'B' || fmt[1] == 'C' || fmt[1] == 'W')) return 0;

    char buf[256];
    va_list args;
    va_start(args, fmt);
    char* p = buf;
    for (const char* f = fmt; *f; f++) {
        if (*f != '%') {
            *p++ = *f;
            continue;
        }
        f++;
        if (*f == 's') {
            const char* s = va_arg(args, const char*);
            if (!s) s = "(null)";
            while (*s && p < buf + 250) *p++ = *s++;
        } else if (*f == 'd') {
            int val = va_arg(args, int);
            if (val < 0) { *p++ = '-'; val = -val; }
            char nbuf[16]; int bi = 0;
            if (val == 0) nbuf[bi++] = '0';
            while (val > 0) { nbuf[bi++] = '0' + (val % 10); val /= 10; }
            while (bi > 0 && p < buf + 250) *p++ = nbuf[--bi];
        } else if (*f == 'c') {
            *p++ = (char)va_arg(args, int);
        }
    }
    *p = '\0';
    va_end(args);
    kprintf("%s", buf);
    return 0;
}

void comp_builtin_print(int64_t val) {
    kprintf("%ld\n", val);
}

void comp_builtin_putc(int64_t c) {
    terminal_putchar((char)c);
}

void comp_builtin_print_str(const char* s) {
    kprintf("%s", s);
}
