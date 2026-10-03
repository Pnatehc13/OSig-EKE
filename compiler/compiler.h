#ifndef COMPILER_H
#define COMPILER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Compiles in-memory C source string directly to x86-64 machine code bytes
int compile_c_code(char* src_text, const char* mod_name, uint32_t mod_type, uint8_t** out_bin, size_t* out_size);

// Reads C source file from filesystem, compiles it, and saves .mod output binary to filesystem
int compile_file(const char* src_path, const char* out_mod_path, const char* mod_name, uint32_t mod_type);

#ifdef __cplusplus
}
#endif

#endif // COMPILER_H
