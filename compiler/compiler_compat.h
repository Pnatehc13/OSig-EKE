#ifndef COMPILER_COMPAT_H
#define COMPILER_COMPAT_H

#include <stdint.h>
#include <stddef.h>
#include "../kernel/tty.h"

#ifdef __cplusplus
extern "C" {
#endif

// 1. Console Output
int comp_printf(const char* fmt, ...);
void comp_builtin_print(int64_t val);
void comp_builtin_putc(int64_t c);
void comp_builtin_print_str(const char* s);
#define printf comp_printf

// 2. Character classification
#define isdigit(c) ((c) >= '0' && (c) <= '9')
#define isalpha(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define isspace(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r')

// 3. String helpers
int comp_strcmp(const char* s1, const char* s2);
size_t comp_strlen(const char* s);
char* comp_strcpy(char* dest, const char* src);
char* comp_strncpy(char* dest, const char* src, size_t n);
void* memset(void* s, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
int comp_sprintf(char* out, const char* fmt, ...);
int comp_atoi(const char* s);
char* comp_strdup(const char* s);

#define strcmp comp_strcmp
#define strlen comp_strlen
#define strcpy comp_strcpy
#define strncpy comp_strncpy
#define sprintf comp_sprintf
#define atoi comp_atoi
#define strdup comp_strdup
#define _strdup comp_strdup

// 4. Memory Arena Allocator
void* comp_calloc(size_t num, size_t size);
void* comp_realloc(void* ptr, size_t new_size);
void comp_free(void* ptr);
void comp_reset_arena();
int comp_is_in_arena(void* ptr);

#define calloc comp_calloc
#define realloc comp_realloc
#define free comp_free
#define malloc(sz) comp_calloc(1, sz)

// 5. Safe Error Recovery (setjmp/longjmp)
typedef uint64_t jmp_buf[8];
int comp_setjmp(jmp_buf env);
void comp_longjmp(jmp_buf env, int val);

extern jmp_buf comp_err_jmp;
extern int comp_has_error;

#define exit(code) do { comp_has_error = (code); comp_longjmp(comp_err_jmp, code ? code : 1); } while(0)

#ifdef __cplusplus
}
#endif

#endif // COMPILER_COMPAT_H
