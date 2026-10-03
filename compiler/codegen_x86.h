#ifndef CODEGEN_X86_H
#define CODEGEN_X86_H

#include "parser.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Module header placed at Byte 0 of .mod / .bin files (64 bytes)
typedef struct {
    uint32_t magic;        // 0x454B4531 ("EKE1")
    char     name[32];     // Module name (null-terminated)
    uint32_t type;         // ModuleType (MT_SCHEDULER, MT_HEAP, etc.)
    uint32_t init_offset;  // Byte offset to entry point in this file (e.g. 64)
    uint32_t code_size;    // Size of generated machine code
    uint32_t reserved;
} DiskModuleHeader;

// Generates x86-64 machine code from parser AST
// Returns pointer to memory buffer containing the complete binary (header + machine code)
uint8_t* codegen_x86_emit(const char* mod_name, uint32_t mod_type, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif // CODEGEN_X86_H
