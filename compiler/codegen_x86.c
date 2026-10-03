#include "codegen_x86.h"
#include "compiler_compat.h"
#include "parser.h"
#include "lexer.h"

#define CODE_BUF_SIZE 65536
static uint8_t code_buf[CODE_BUF_SIZE];
static size_t code_len = 0;

// 1. Primitive Byte Emitters
static void emit_byte(uint8_t b) {
    if (code_len < CODE_BUF_SIZE) {
        code_buf[code_len++] = b;
    }
}

static void emit_bytes(const uint8_t* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        emit_byte(b[i]);
    }
}

static void emit_u32(uint32_t val) {
    emit_byte((uint8_t)(val & 0xFF));
    emit_byte((uint8_t)((val >> 8) & 0xFF));
    emit_byte((uint8_t)((val >> 16) & 0xFF));
    emit_byte((uint8_t)((val >> 24) & 0xFF));
}

static void emit_u64(uint64_t val) {
    emit_u32((uint32_t)(val & 0xFFFFFFFF));
    emit_u32((uint32_t)((val >> 32) & 0xFFFFFFFF));
}

// 2. x86-64 Stack Machine Instructions
static void emit_push_rax() { emit_byte(0x50); }
static void emit_push_rbx() { emit_byte(0x53); }
static void emit_pop_rax()  { emit_byte(0x58); }
static void emit_pop_rbx()  { emit_byte(0x5B); }

static void emit_mov_imm_rax(int64_t val) {
    emit_byte(0x48); emit_byte(0xB8); // movabsq $val, %rax
    emit_u64((uint64_t)val);
}

static void emit_add_rax_rbx() {
    emit_byte(0x48); emit_byte(0x01); emit_byte(0xD8); // add %rbx, %rax
}

static void emit_sub_rax_rbx() {
    emit_byte(0x48); emit_byte(0x29); emit_byte(0xD8); // sub %rbx, %rax
}

static void emit_imul_rax_rbx() {
    emit_byte(0x48); emit_byte(0x0F); emit_byte(0xAF); emit_byte(0xC3); // imul %rbx, %rax
}

static void emit_idiv_rax_rbx() {
    emit_byte(0x48); emit_byte(0x99); // cqo (sign-extend rax into rdx:rax)
    emit_byte(0x48); emit_byte(0xF7); emit_byte(0xFB); // idiv %rbx
}

// Load local variable into rax: mov -offset(%rbp), %rax
static void emit_load_local(int offset) {
    int32_t disp;
    if (offset < 0) {
        int arg_idx = (-12 - offset) / 4;
        disp = 16 + arg_idx * 8;
        // 64-bit parameter load from stack: mov 16(%rbp), %rax
        emit_byte(0x48); emit_byte(0x8B); emit_byte(0x85);
        emit_u32((uint32_t)disp);
    } else {
        disp = -(offset + 8);
        // 32-bit local load: mov disp32(%rbp), %eax (auto zero-extends into %rax)
        emit_byte(0x8B); emit_byte(0x85);
        emit_u32((uint32_t)disp);
    }
    emit_push_rax();
}

// Store rax into local variable: mov %rax, -offset(%rbp)
static void emit_store_local(int offset) {
    emit_pop_rax();
    int32_t disp;
    if (offset < 0) {
        int arg_idx = (-12 - offset) / 4;
        disp = 16 + arg_idx * 8;
        // 64-bit parameter store: mov %rax, 16(%rbp)
        emit_byte(0x48); emit_byte(0x89); emit_byte(0x85);
        emit_u32((uint32_t)disp);
    } else {
        disp = -(offset + 8);
        // 32-bit local store: mov %eax, disp32(%rbp) (writes exactly 4 bytes)
        emit_byte(0x89); emit_byte(0x85);
        emit_u32((uint32_t)disp);
    }
}

// Memory dereference: PEEK (*addr -> rax)
static void emit_peek() {
    emit_pop_rbx(); // rbx = addr
    emit_byte(0x48); emit_byte(0x8B); emit_byte(0x03); // mov (%rbx), %rax
    emit_push_rax();
}

// Memory store: POKE (*addr = val)
static void emit_poke() {
    emit_pop_rax(); // rax = value
    emit_pop_rbx(); // rbx = addr
    emit_byte(0x48); emit_byte(0x89); emit_byte(0x03); // mov %rax, (%rbx)
}

// Function prologue and epilogue
static void emit_prologue(int local_bytes) {
    emit_byte(0x55); // push %rbp
    emit_byte(0x48); emit_byte(0x89); emit_byte(0xE5); // mov %rsp, %rbp
    local_bytes = (local_bytes + 15) & ~15; // 16-byte stack align
    if (local_bytes > 0) {
        emit_byte(0x48); emit_byte(0x81); emit_byte(0xEC); // sub $local_bytes, %rsp
        emit_u32((uint32_t)local_bytes);
    }
}

static void emit_epilogue() {
    emit_byte(0xC9); // leave
    emit_byte(0xC3); // ret
}

// 3. Labels and Fixup Backpatching
#define MAX_LABELS 512
static int label_pos[MAX_LABELS];
static int next_label_id = 1;

static int find_func_offset(const char* name);

typedef struct {
    int label_id;
    size_t patch_pos;
    int is_func_call;
    char func_name[64];
} Fixup;

#define MAX_FIXUPS 1024
static Fixup fixups[MAX_FIXUPS];
static int num_fixups = 0;

static int alloc_label() {
    int id = next_label_id++;
    if (id < MAX_LABELS) {
        label_pos[id] = -1;
    }
    return id;
}

static void mark_label(int id) {
    if (id < MAX_LABELS) {
        label_pos[id] = (int)code_len;
    }
}

static void emit_jmp(int label_id) {
    emit_byte(0xE9); // jmp rel32
    if (num_fixups < MAX_FIXUPS) {
        fixups[num_fixups].label_id = label_id;
        fixups[num_fixups].patch_pos = code_len;
        fixups[num_fixups].is_func_call = 0;
        num_fixups++;
    }
    emit_u32(0); // placeholder
}

static void emit_cond_jump(int op, int label_id) {
    // Top of stack: left, right
    emit_pop_rbx(); // rbx = right
    emit_pop_rax(); // rax = left
    emit_byte(0x48); emit_byte(0x39); emit_byte(0xD8); // cmp %rbx, %rax

    // Jump if condition is TRUE
    switch (op) {
        case '<':   emit_byte(0x0F); emit_byte(0x8C); break; // jl
        case '>':   emit_byte(0x0F); emit_byte(0x8F); break; // jg
        case T_LEQ: emit_byte(0x0F); emit_byte(0x8E); break; // jle
        case T_GEQ: emit_byte(0x0F); emit_byte(0x8D); break; // jge
        case T_EQ:  emit_byte(0x0F); emit_byte(0x84); break; // je
        case T_NEQ: emit_byte(0x0F); emit_byte(0x85); break; // jne
        default:    emit_byte(0x0F); emit_byte(0x85); break; // default jne
    }

    if (num_fixups < MAX_FIXUPS) {
        fixups[num_fixups].label_id = label_id;
        fixups[num_fixups].patch_pos = code_len;
        fixups[num_fixups].is_func_call = 0;
        num_fixups++;
    }
    emit_u32(0); // placeholder
}

static void patch_fixups() {
    for (int i = 0; i < num_fixups; i++) {
        if (fixups[i].is_func_call) {
            int target = find_func_offset(fixups[i].func_name);
            if (target != -1) {
                int32_t from = (int32_t)(fixups[i].patch_pos + 4);
                int32_t rel = target - from;
                *(int32_t*)(&code_buf[fixups[i].patch_pos]) = rel;
            } else {
                kprintf("[CODEGEN] Warning: Undefined function '%s'\n", fixups[i].func_name);
            }
        } else {
            int id = fixups[i].label_id;
            if (id < MAX_LABELS && label_pos[id] != -1) {
                int32_t target = label_pos[id];
                int32_t from = (int32_t)(fixups[i].patch_pos + 4);
                int32_t rel = target - from;
                *(int32_t*)(&code_buf[fixups[i].patch_pos]) = rel;
            }
        }
    }
}

// 4. Function Symbols Table
typedef struct {
    char name[64];
    size_t offset;
} FuncSym;

static FuncSym func_table[128];
static int num_funcs = 0;

static void register_func(const char* name, size_t offset) {
    if (num_funcs < 128) {
        strncpy(func_table[num_funcs].name, name, 63);
        func_table[num_funcs].name[63] = '\0';
        func_table[num_funcs].offset = offset;
        num_funcs++;
    }
}

static int find_func_offset(const char* name) {
    for (int i = 0; i < num_funcs; i++) {
        if (strcmp(func_table[i].name, name) == 0) {
            return (int)func_table[i].offset;
        }
    }
    return -1;
}

// 4b. String Pool & Fixups
#define MAX_STR_POOL 8192
static char str_pool[MAX_STR_POOL];
static size_t str_pool_len = 0;

typedef struct {
    size_t lea_disp_pos;
    int str_pool_offset;
} StrFixup;

#define MAX_STR_FIXUPS 256
static StrFixup str_fixups[MAX_STR_FIXUPS];
static int num_str_fixups = 0;

static int add_to_string_pool(const char* raw_str) {
    if (!raw_str) return 0;
    int start_offset = (int)str_pool_len;
    int i = 0;
    if (raw_str[0] == '"') i++;
    int end = (int)strlen(raw_str);
    if (end > i && raw_str[end - 1] == '"') end--;

    while (i < end && str_pool_len < MAX_STR_POOL - 2) {
        if (raw_str[i] == '\\' && i + 1 < end) {
            i++;
            if (raw_str[i] == 'n') str_pool[str_pool_len++] = '\n';
            else if (raw_str[i] == 't') str_pool[str_pool_len++] = '\t';
            else if (raw_str[i] == 'r') str_pool[str_pool_len++] = '\r';
            else if (raw_str[i] == '\\') str_pool[str_pool_len++] = '\\';
            else if (raw_str[i] == '\"') str_pool[str_pool_len++] = '\"';
            else if (raw_str[i] == '0') str_pool[str_pool_len++] = '\0';
            else str_pool[str_pool_len++] = raw_str[i];
            i++;
        } else {
            str_pool[str_pool_len++] = raw_str[i++];
        }
    }
    str_pool[str_pool_len++] = '\0';
    return start_offset;
}

static void patch_str_fixups(size_t str_section_start) {
    for (int i = 0; i < num_str_fixups; i++) {
        size_t patch_pos = str_fixups[i].lea_disp_pos;
        size_t next_ip = patch_pos + 4;
        size_t target_str = str_section_start + str_fixups[i].str_pool_offset;
        int32_t disp = (int32_t)(target_str - next_ip);
        *(int32_t*)(&code_buf[patch_pos]) = disp;
    }
}

// 5. AST Traversal
static void gen_code_x86(Node* n);

static void gen_condition(Node* cond, int target_label, int jump_on_true) {
    if (!cond) return;
    if (cond->type == NODE_BIN) {
        gen_code_x86(cond->bin.left);
        gen_code_x86(cond->bin.right);
        int op = cond->bin.op;
        if (!jump_on_true) {
            // Invert condition
            switch (op) {
                case '<':   op = T_GEQ; break;
                case '>':   op = T_LEQ; break;
                case T_LEQ: op = '>';   break;
                case T_GEQ: op = '<';   break;
                case T_EQ:  op = T_NEQ; break;
                case T_NEQ: op = T_EQ;  break;
            }
        }
        emit_cond_jump(op, target_label);
    } else {
        gen_code_x86(cond);
        emit_pop_rax();
        emit_byte(0x48); emit_byte(0x85); emit_byte(0xC0); // test %rax, %rax
        if (jump_on_true) {
            emit_byte(0x0F); emit_byte(0x85); // jne
        } else {
            emit_byte(0x0F); emit_byte(0x84); // je
        }
        if (num_fixups < MAX_FIXUPS) {
            fixups[num_fixups].label_id = target_label;
            fixups[num_fixups].patch_pos = code_len;
            num_fixups++;
        }
        emit_u32(0);
    }
}

static void gen_code_x86(Node* n) {
    if (!n) return;

    switch (n->type) {
        case NODE_INT:
            emit_mov_imm_rax(n->int_val);
            emit_push_rax();
            break;

        case NODE_VAR:
            emit_load_local(n->var.offset);
            break;

        case NODE_BIN:
            gen_code_x86(n->bin.left);
            gen_code_x86(n->bin.right);
            emit_pop_rbx(); // right
            emit_pop_rax(); // left
            switch (n->bin.op) {
                case '+': emit_add_rax_rbx(); break;
                case '-': emit_sub_rax_rbx(); break;
                case '*': emit_imul_rax_rbx(); break;
                case '/': emit_idiv_rax_rbx(); break;
            }
            emit_push_rax();
            break;

        case NODE_ASSIGN:
            gen_code_x86(n->bin.right); // compute value
            if (n->bin.left->type == NODE_VAR) {
                int off = n->var.offset ? n->var.offset : n->bin.left->var.offset;
                emit_store_local(off);
            } else if (n->bin.left->type == NODE_POINTER) {
                gen_code_x86(n->bin.left->pointer.value); // addr
                emit_poke();
            }
            break;

        case NODE_RETURN:
            if (n->ret.value) {
                gen_code_x86(n->ret.value);
                emit_pop_rax();
            } else {
                emit_byte(0x31); emit_byte(0xC0); // xor %eax, %eax
            }
            emit_epilogue();
            break;

        case NODE_IF: {
            int lbl_else = alloc_label();
            int lbl_exit = alloc_label();

            // Jump to else if condition is FALSE
            gen_condition(n->flow.cond, n->flow.else_stmt ? lbl_else : lbl_exit, 0);
            gen_code_x86(n->flow.then_stmt);

            if (n->flow.else_stmt) {
                emit_jmp(lbl_exit);
                mark_label(lbl_else);
                gen_code_x86(n->flow.else_stmt);
            }
            mark_label(lbl_exit);
            break;
        }

        case NODE_WHILE: {
            int lbl_start = alloc_label();
            int lbl_exit = alloc_label();

            mark_label(lbl_start);
            // Jump to exit if condition is FALSE
            gen_condition(n->flow.cond, lbl_exit, 0);
            gen_code_x86(n->flow.then_stmt);
            emit_jmp(lbl_start);
            mark_label(lbl_exit);
            break;
        }

        case NODE_BLOCK: {
            Node* stmt = n->func.body;
            while (stmt) {
                if (stmt->type == NODE_VAR) {
                    if (stmt->var.value) {
                        gen_code_x86(stmt->var.value);
                        emit_store_local(stmt->var.offset);
                    }
                } else {
                    gen_code_x86(stmt);
                }
                stmt = stmt->next;
            }
            break;
        }

        case NODE_CALL: {
            Node* args[32];
            int arg_count = 0;
            Node* curr = n->func.args;
            while (curr && arg_count < 32) {
                args[arg_count++] = curr;
                curr = curr->next;
            }

            if (strcmp(n->func.name, "printf") == 0) {
                if (arg_count == 1) {
                    gen_code_x86(args[0]);
                    emit_byte(0x5F); // pop %rdi
                    emit_byte(0x48); emit_byte(0xBB); // movabs $comp_builtin_print_str, %rbx
                    emit_u64((uint64_t)&comp_builtin_print_str);
                    emit_byte(0xFF); emit_byte(0xD3); // call *%rbx
                    emit_push_rax();
                } else if (arg_count > 1) {
                    for (int i = arg_count - 1; i >= 0; i--) {
                        gen_code_x86(args[i]);
                    }
                    if (arg_count >= 1) emit_byte(0x5F); // pop %rdi
                    if (arg_count >= 2) emit_byte(0x5E); // pop %rsi
                    if (arg_count >= 3) emit_byte(0x5A); // pop %rdx
                    if (arg_count >= 4) emit_byte(0x59); // pop %rcx
                    if (arg_count >= 5) { emit_byte(0x41); emit_byte(0x58); } // pop %r8
                    if (arg_count >= 6) { emit_byte(0x41); emit_byte(0x59); } // pop %r9
                    emit_byte(0x31); emit_byte(0xC0); // xor %eax, %eax
                    emit_byte(0x48); emit_byte(0xBB); // movabs $kprintf, %rbx
                    emit_u64((uint64_t)&kprintf);
                    emit_byte(0xFF); emit_byte(0xD3); // call *%rbx
                    emit_push_rax();
                } else {
                    emit_push_rax();
                }
                break;
            } else if (strcmp(n->func.name, "print") == 0 || strcmp(n->func.name, "kprintf") == 0) {
                if (arg_count > 0) {
                    gen_code_x86(args[0]);
                    emit_byte(0x5F); // pop %rdi
                } else {
                    emit_byte(0x48); emit_byte(0x31); emit_byte(0xFF); // xor %rdi, %rdi
                }
                emit_byte(0x48); emit_byte(0xBB); // movabs $comp_builtin_print, %rbx
                emit_u64((uint64_t)&comp_builtin_print);
                emit_byte(0xFF); emit_byte(0xD3); // call *%rbx
                emit_push_rax();
                break;
            } else if (strcmp(n->func.name, "putc") == 0) {
                if (arg_count > 0) {
                    gen_code_x86(args[0]);
                    emit_byte(0x5F); // pop %rdi
                } else {
                    emit_byte(0x48); emit_byte(0x31); emit_byte(0xFF); // xor %rdi, %rdi
                }
                emit_byte(0x48); emit_byte(0xBB); // movabs $comp_builtin_putc, %rbx
                emit_u64((uint64_t)&comp_builtin_putc);
                emit_byte(0xFF); emit_byte(0xD3); // call *%rbx
                emit_push_rax();
                break;
            }

            // Normal function call: push arguments in reverse order (last to first)
            for (int i = arg_count - 1; i >= 0; i--) {
                gen_code_x86(args[i]);
            }

            int target = find_func_offset(n->func.name);
            if (target != -1) {
                emit_byte(0xE8); // call rel32
                int32_t rel = target - (int32_t)(code_len + 4);
                emit_u32((uint32_t)rel);
            } else {
                emit_byte(0xE8); // call rel32
                if (num_fixups < MAX_FIXUPS) {
                    fixups[num_fixups].is_func_call = 1;
                    strncpy(fixups[num_fixups].func_name, n->func.name, 63);
                    fixups[num_fixups].func_name[63] = '\0';
                    fixups[num_fixups].patch_pos = code_len;
                    num_fixups++;
                }
                emit_u32(0); // placeholder
            }

            if (arg_count > 0) {
                emit_byte(0x48); emit_byte(0x81); emit_byte(0xC4); // add $(arg_count*8), %rsp
                emit_u32((uint32_t)(arg_count * 8));
            }
            emit_push_rax(); // push return value
            break;
        }

        case NODE_POINTER:
            gen_code_x86(n->pointer.value);
            emit_peek();
            break;

        case NODE_STR: {
            int str_offset = add_to_string_pool(n->str.string);
            emit_byte(0x48); emit_byte(0x8D); emit_byte(0x05); // lea disp32(%rip), %rax
            if (num_str_fixups < MAX_STR_FIXUPS) {
                str_fixups[num_str_fixups].lea_disp_pos = code_len;
                str_fixups[num_str_fixups].str_pool_offset = str_offset;
                num_str_fixups++;
            }
            emit_u32(0); // placeholder for disp32
            emit_push_rax(); // push %rax (the string pointer!)
            break;
        }

        default:
            break;
    }
}

// 6. Main Code Generation Driver
uint8_t* codegen_x86_emit(const char* mod_name, uint32_t mod_type, size_t* out_len) {
    code_len = 0;
    num_fixups = 0;
    num_funcs = 0;
    str_pool_len = 0;
    num_str_fixups = 0;
    next_label_id = 1;

    for (int i = 0; i < MAX_LABELS; i++) {
        label_pos[i] = -1;
    }

    // 1. Reserve Byte 0..63 for DiskModuleHeader
    DiskModuleHeader* hdr = (DiskModuleHeader*)code_buf;
    code_len = 64;

    // 2. Emit all top-level functions from parser's global_table
    for (int i = 0; i < gt_count; i++) {
        if (global_table[i].p && global_table[i].p->type == NODE_FUNC) {
            Node* fn = global_table[i].p;
            register_func(fn->func.name, code_len);

            // Function prologue
            emit_prologue(fn->func.localvarbyte + 32);

            // Function body
            if (fn->func.body) {
                gen_code_x86(fn->func.body);
            }

            // Default epilogue in case return was omitted
            emit_epilogue();
        }
    }

    // 3. Emit string pool data section
    while (code_len % 8 != 0) emit_byte(0x90); // nop align
    size_t str_section_start = code_len;
    for (size_t i = 0; i < str_pool_len; i++) {
        emit_byte((uint8_t)str_pool[i]);
    }

    // 4. Resolve all jumps & string references
    patch_fixups();
    patch_str_fixups(str_section_start);

    // 5. Fill in DiskModuleHeader at Byte 0
    hdr->magic = 0x454B4531; // "EKE1"
    strncpy(hdr->name, mod_name ? mod_name : "UnnamedMod", 31);
    hdr->name[31] = '\0';
    hdr->type = mod_type;
    int main_offset = find_func_offset("main");
    if (main_offset == -1) main_offset = find_func_offset("module_init");
    if (main_offset == -1) main_offset = find_func_offset("init");
    if (main_offset == -1) main_offset = 64; // default to first function
    hdr->init_offset = (uint32_t)main_offset;
    hdr->code_size = (uint32_t)(code_len - 64);
    hdr->reserved = 0;

    if (out_len) {
        *out_len = code_len;
    }

    return code_buf;
}
