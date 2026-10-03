#include "compiler.h"
#include "compiler_compat.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "codegen_x86.h"
#include "../kernel/kernel_api.h"
#include "../kernel/module_api.h"

extern struct Kernel gk;

int compile_c_code(char* src_text, const char* mod_name, uint32_t mod_type, uint8_t** out_bin, size_t* out_size) {
    if (!comp_is_in_arena(src_text)) {
        comp_reset_arena();
    }
    comp_has_error = 0;

    // Set up safe abort point for parser/sema errors
    if (comp_setjmp(comp_err_jmp) != 0) {
        kprintf("[COMPILER] Compilation aborted due to errors.\n");
        return -1;
    }

    // 1. Lexer
    Token* toks = init_lexer(src_text);
    if (!toks) {
        kprintf("[COMPILER] Error: Lexer failed.\n");
        return -1;
    }

    // 2. Parser
    init_parser();
    parse_top_level();

    // 3. Semantic Analysis
    sema_run_global_analysis();

    // 4. Code Generation
    uint8_t* bin = codegen_x86_emit(mod_name, mod_type, out_size);
    if (!bin || *out_size == 0) {
        kprintf("[COMPILER] Error: Code generation failed.\n");
        return -1;
    }

    if (out_bin) *out_bin = bin;
    return 0;
}

int compile_file(const char* src_path, const char* out_mod_path, const char* mod_name, uint32_t mod_type) {
    if (!gk.fs) {
        kprintf("[COMPILER] Error: No active filesystem!\n");
        return -1;
    }

    comp_reset_arena();

    uint64_t file_sz = gk.fs->get_size(src_path);
    if (file_sz == 0) {
        kprintf("[COMPILER] Error: Source file '%s' not found or empty.\n", src_path);
        return -1;
    }

    char* src_buf = (char*)comp_calloc(1, file_sz + 1);
    int read_bytes = gk.fs->read_file(src_path, src_buf, file_sz);
    if (read_bytes < 0) {
        kprintf("[COMPILER] Error: Failed to read '%s'.\n", src_path);
        return -1;
    }
    src_buf[read_bytes] = '\0';

    kprintf("[COMPILER] Read %d bytes from '%s'. Compiling...\n", read_bytes, src_path);

    uint8_t* out_bin = 0;
    size_t out_len = 0;
    if (compile_c_code(src_buf, mod_name, mod_type, &out_bin, &out_len) != 0) {
        return -1;
    }

    if (gk.fs->write_file(out_mod_path, out_bin, out_len) != 0) {
        kprintf("[COMPILER] Error: Failed to write output file '%s'.\n", out_mod_path);
        return -1;
    }

    kprintf("[COMPILER] Success! Wrote %d bytes to '%s'.\n", (uint32_t)out_len, out_mod_path);
    return 0;
}
