#include "shell.h"
#include "tty.h"
#include "../drivers/keyboard.h"
#include "kernel_api.h"
#include "module_api.h"
#include "../storage/spmm.h"
#include "module_swap.h"
#include "../compiler/compiler.h"
#include "../compiler/codegen_x86.h"
#include "../editor/editor.h"

extern struct Kernel gk;

bool str_eq(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++; b++;
    }
    return *a == *b;
}

static uint64_t str_len(const char* s) {
    uint64_t len = 0;
    while (s && s[len]) len++;
    return len;
}

static void str_copy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

static bool next_token(const char*& p, char* buf, int max_len) {
    while (*p == ' ') p++;
    if (*p == '\0') return false;
    int i = 0;
    while (*p && *p != ' ' && i < max_len - 1) {
        buf[i++] = *p++;
    }
    buf[i] = '\0';
    while (*p == ' ') p++;
    return true;
}

// Command definition
struct ShellCommand {
    const char* name;
    const char* description;
    void (*handler)(const char* args);
};

// Forward declare command handlers
static void cmd_help(const char* args);
static void cmd_clear(const char* args);
static void cmd_mem(const char* args);
static void cmd_echo(const char* args);
static void cmd_mods(const char* args);
static void cmd_load(const char* args);
static void cmd_disks(const char* args);
static void cmd_drive(const char* args);
static void cmd_ls(const char* args);
static void cmd_cat(const char* args);
static void cmd_write(const char* args);
static void cmd_rm(const char* args);
static void cmd_compile(const char* args);
static void cmd_makemod(const char* args);
static void cmd_run(const char* args);
static void cmd_edit(const char* args);

// The Command Dispatch Table
static const ShellCommand commands[] = {
    {"help",    "Show this help menu",        cmd_help},
    {"clear",   "Clear terminal screen",      cmd_clear},
    {"mem",     "Show heap allocations",      cmd_mem},
    {"echo",    "Print text to screen",       cmd_echo},
    {"mods",    "List modules by category",   cmd_mods},
    {"load",    "Hot-swap an active module",  cmd_load},
    {"disks",   "List registered drives",     cmd_disks},
    {"drive",   "Switch active drive",        cmd_drive},
    {"ls",      "List files on drive",        cmd_ls},
    {"cat",     "Read and display a file",   cmd_cat},
    {"write",   "Write text to a file",       cmd_write},
    {"rm",      "Delete a file from drive",  cmd_rm},
    {"edit",    "Full-screen on-OS editor",   cmd_edit},
    {"compile", "Compile C source to .mod",   cmd_compile},
    {"makemod", "Build & register kernel mod",cmd_makemod},
    {"run",     "Execute a binary program",   cmd_run}
};

static const uint32_t num_commands = sizeof(commands) / sizeof(commands[0]);

// 1. HELP
static void cmd_help(const char* args) {
    kprintf("Available Commands:\n");
    for (uint32_t i = 0; i < num_commands; i++) {
        kprintf("  %s - %s\n", commands[i].name, commands[i].description);
    }
}

// 2. CLEAR
static void cmd_clear(const char* args) {
    terminal_initialize();
}

// 3. MEM
static void cmd_mem(const char* args) {
    kprintf("Heap Allocations: %d\n", gk.heapcnt);
}

// 4. ECHO
static void cmd_echo(const char* args) {
    kprintf("%s\n", args);
}

// 5. MODS
static void print_category(const char* cat_name, struct ModuleHeader* mods[], uint32_t count) {
    kprintf("[%s] (%d/8):\n", cat_name, count);
    if (count == 0) {
        kprintf("  (none)\n");
        return;
    }
    for (uint32_t i = 0; i < count; i++) {
        if (mods[i]) {
            kprintf("  [%d] %s\n", i, mods[i]->name);
        }
    }
}

static void cmd_mods(const char* args) {
    kprintf("=== Registered Modules by Category ===\n");
    print_category("SCHEDULER", gk.sched_mods, gk.sched_mod_count);
    print_category("HEAP", gk.heap_mods, gk.heap_mod_count);
    print_category("PMM", gk.pmm_mods, gk.pmm_mod_count);
    print_category("BLOCKDEV", gk.block_mods, gk.block_mod_count);
    print_category("FILESYSTEM", gk.fs_mods, gk.fs_mod_count);
}

// 6. LOAD
static void cmd_load(const char* args) {
    if (!args || args[0] == '\0') {
        kprintf("Usage: load <module_name>\n");
        return;
    }

    // Search registered in-memory modules
    for (uint32_t i = 0; i < gk.total_mod_count; i++) {
        if (gk.all_mods[i] && str_eq(args, gk.all_mods[i]->name)) {
            swap_module(gk.all_mods[i]);
            return;
        }
    }

    kprintf("Module '%s' not found. Type 'mods' to see loaded modules.\n", args);
}

// 7. DISKS
static void cmd_disks(const char* args) {
    kprintf("Registered Disks (%d):\n", gk.disk_count);
    for (uint32_t i = 0; i < gk.disk_count; i++) {
        if (!gk.disks[i]) continue;
        uint32_t mb = (uint32_t)(gk.disks[i]->total_sectors * 512 / (1024 * 1024));
        kprintf("  [%d] %s: %d sectors (%d MB)\n",
                i, gk.disks[i]->name, (uint32_t)gk.disks[i]->total_sectors, mb);
    }
    if (gk.current_disk) {
        kprintf("Active Drive: %s\n", gk.current_disk->name);
    } else {
        kprintf("No active drive selected.\n");
    }
}

// 6. DRIVE
static void cmd_drive(const char* args) {
    if (!args || args[0] == '\0') {
        kprintf("Usage: drive <disk_name> (e.g. drive ram0)\n");
        return;
    }
    if (changedrive(args)) {
        if (gk.fs && gk.current_disk) {
            gk.fs->mount(gk.current_disk);
        }
    }
}

// 7. LS
static void cmd_ls(const char* args) {
    if (!gk.fs) {
        kprintf("Error: No active filesystem!\n");
        return;
    }
    const char* drive_name = gk.current_disk ? gk.current_disk->name : "none";
    kprintf("Files on %s:\n", drive_name);
    int count = gk.fs->list_files([](const char* name, uint64_t size) {
        kprintf("  %s (%d bytes)\n", name, (uint32_t)size);
    });
    if (count == 0) {
        kprintf("  (no files found)\n");
    }
}

// 8. CAT
static void cmd_cat(const char* args) {
    if (!args || args[0] == '\0') {
        kprintf("Usage: cat <filename>\n");
        return;
    }
    if (!gk.fs) {
        kprintf("Error: No active filesystem!\n");
        return;
    }
    uint64_t sz = gk.fs->get_size(args);
    if (sz == 0) {
        kprintf("File '%s' not found or empty\n", args);
        return;
    }
    char buf[513];
    int read_bytes = gk.fs->read_file(args, buf, 512);
    if (read_bytes >= 0) {
        buf[read_bytes] = '\0';
        kprintf("%s\n", buf);
    } else {
        kprintf("Error: Failed to read '%s'\n", args);
    }
}

// 9. WRITE
static void cmd_write(const char* args) {
    if (!args || args[0] == '\0') {
        kprintf("Usage: write <filename> <text>\n");
        return;
    }
    if (!gk.fs) {
        kprintf("Error: No active filesystem!\n");
        return;
    }

    // Split filename and text content
    char fname[32];
    int i = 0;
    while (args[i] && args[i] != ' ' && i < 31) {
        fname[i] = args[i];
        i++;
    }
    fname[i] = '\0';

    if (args[i] != ' ') {
        kprintf("Usage: write <filename> <text>\n");
        return;
    }

    const char* text = args + i + 1;
    uint64_t len = str_len(text);

    if (gk.fs->write_file(fname, text, len) == 0) {
        kprintf("Saved %d bytes to '%s'\n", (uint32_t)len, fname);
    } else {
        kprintf("Error: Could not write file '%s'\n", fname);
    }
}

// 10. RM
static void cmd_rm(const char* args) {
    if (!args || args[0] == '\0') {
        kprintf("Usage: rm <filename>\n");
        return;
    }
    if (!gk.fs) {
        kprintf("Error: No active filesystem!\n");
        return;
    }
    if (gk.fs->delete_file(args) == 0) {
        kprintf("Deleted '%s'\n", args);
    } else {
        kprintf("Error: File '%s' not found\n", args);
    }
}

// 11. COMPILE
static void cmd_compile(const char* args) {
    if (!args || args[0] == '\0') {
        kprintf("Usage: compile <source.c> [out.mod]\n");
        return;
    }
    if (!gk.fs) {
        kprintf("Error: No active filesystem!\n");
        return;
    }

    const char* p = args;
    char src[32];
    char out[32];
    if (!next_token(p, src, 32)) {
        kprintf("Usage: compile <source.c> [out.mod]\n");
        return;
    }
    if (!next_token(p, out, 32)) {
        str_copy(out, "a.out");
    }

    char mod_name[32];
    int i = 0;
    while (src[i] && src[i] != '.' && i < 31) {
        mod_name[i] = src[i];
        i++;
    }
    mod_name[i] = '\0';

    kprintf("[SHELL] Compiling '%s' -> '%s'...\n", src, out);
    if (compile_file(src, out, mod_name, 0) == 0) {
        kprintf("[SHELL] Done! Run with: ./%s\n", out);
    }
}

// 12. RUN
static void cmd_run(const char* args) {
    while (args && *args == ' ') args++;
    if (!args || args[0] == '\0') {
        kprintf("Usage: run <binary> or ./<binary>\n");
        return;
    }
    if (!gk.fs) {
        kprintf("Error: No active filesystem!\n");
        return;
    }

    uint64_t sz = gk.fs->get_size(args);
    if (sz == 0) {
        kprintf("Error: File '%s' not found or empty.\n", args);
        return;
    }

    int pages = (int)((sz + 4095) / 4096);
    uintptr_t mem = gk.pmm->alloc_page(pages);
    if (!mem) {
        kprintf("Error: Out of memory allocating %d page(s) for '%s'\n", pages, args);
        return;
    }

    int read_bytes = gk.fs->read_file(args, (void*)mem, sz);
    if (read_bytes < 0) {
        kprintf("Error: Failed to read file '%s'\n", args);
        for (int i = 0; i < pages; i++) gk.pmm->free_page(mem + i * 4096);
        return;
    }

    DiskModuleHeader* dhdr = (DiskModuleHeader*)mem;
    uint8_t* entry_ptr = (uint8_t*)mem;
    if (dhdr->magic == MAGICNUM) {
        entry_ptr += dhdr->init_offset;
    }

    kprintf("[EXEC] Starting '%s'...\n", args);
    typedef int (*entry_fn_t)(void);
    entry_fn_t entry = (entry_fn_t)entry_ptr;
    int ret = entry();
    kprintf("[EXEC] '%s' returned: %d\n", args, ret);

    for (int i = 0; i < pages; i++) {
        gk.pmm->free_page(mem + i * 4096);
    }
}

// 13. MAKEMOD
static void cmd_makemod(const char* args) {
    if (!args || args[0] == '\0') {
        kprintf("Usage: makemod <file.c | file.mod> <type: sched|heap|pmm|fs|disk> [name]\n");
        return;
    }
    if (!gk.fs) {
        kprintf("Error: No active filesystem!\n");
        return;
    }

    const char* p = args;
    char file[32];
    char type_str[32];
    char name[32];

    if (!next_token(p, file, 32)) {
        kprintf("Usage: makemod <file.c | file.mod> <type> [name]\n");
        return;
    }
    if (!next_token(p, type_str, 32)) {
        kprintf("Error: Module type required (sched, heap, pmm, fs, disk)\n");
        return;
    }
    if (!next_token(p, name, 32)) {
        int i = 0;
        while (file[i] && file[i] != '.' && i < 31) {
            name[i] = file[i];
            i++;
        }
        name[i] = '\0';
    }

    uint32_t mod_type = 0;
    if (str_eq(type_str, "sched") || str_eq(type_str, "scheduler") || str_eq(type_str, "1")) {
        mod_type = MT_SCHEDULER;
    } else if (str_eq(type_str, "heap") || str_eq(type_str, "2")) {
        mod_type = MT_HEAP;
    } else if (str_eq(type_str, "pmm") || str_eq(type_str, "3")) {
        mod_type = MT_PMM;
    } else if (str_eq(type_str, "ipc") || str_eq(type_str, "4")) {
        mod_type = MT_IPC;
    } else if (str_eq(type_str, "fs") || str_eq(type_str, "filesystem") || str_eq(type_str, "5")) {
        mod_type = MT_FS;
    } else if (str_eq(type_str, "disk") || str_eq(type_str, "block") || str_eq(type_str, "6")) {
        mod_type = MT_BLOCKDEV;
    } else {
        kprintf("Error: Unknown module type '%s'. Valid: sched, heap, pmm, fs, disk\n", type_str);
        return;
    }

    char mod_file[36];
    uint64_t flen = str_len(file);
    if (flen > 2 && file[flen - 2] == '.' && file[flen - 1] == 'c') {
        int j = 0;
        while (name[j] && j < 27) {
            mod_file[j] = name[j];
            j++;
        }
        mod_file[j++] = '.';
        mod_file[j++] = 'm';
        mod_file[j++] = 'o';
        mod_file[j++] = 'd';
        mod_file[j] = '\0';

        kprintf("[MAKEMOD] Compiling '%s' -> '%s' (type %d)...\n", file, mod_file, mod_type);
        if (compile_file(file, mod_file, name, mod_type) != 0) {
            kprintf("Error: Failed to compile module.\n");
            return;
        }
    } else {
        str_copy(mod_file, file);
    }

    uint64_t sz = gk.fs->get_size(mod_file);
    if (sz == 0) {
        kprintf("Error: Module file '%s' not found or empty.\n", mod_file);
        return;
    }

    int pages = (int)((sz + 4095) / 4096);
    uintptr_t mem = gk.pmm->alloc_page(pages);
    if (!mem) {
        kprintf("Error: Out of memory loading module!\n");
        return;
    }

    int read_bytes = gk.fs->read_file(mod_file, (void*)mem, sz);
    if (read_bytes < 0) {
        kprintf("Error: Failed to read '%s'\n", mod_file);
        for (int i = 0; i < pages; i++) gk.pmm->free_page(mem + i * 4096);
        return;
    }

    DiskModuleHeader* dhdr = (DiskModuleHeader*)mem;
    if (dhdr->magic != MAGICNUM) {
        kprintf("Error: File '%s' is not a valid EKE module (bad magic 0x%X)!\n", mod_file, dhdr->magic);
        for (int i = 0; i < pages; i++) gk.pmm->free_page(mem + i * 4096);
        return;
    }

    if (gk.total_mod_count >= 32) {
        kprintf("Error: Maximum module limit (32) reached!\n");
        for (int i = 0; i < pages; i++) gk.pmm->free_page(mem + i * 4096);
        return;
    }

    ModuleHeader* mod = (ModuleHeader*)gk.heap->kmalloc(sizeof(ModuleHeader));
    if (!mod) {
        kprintf("Error: Failed to allocate ModuleHeader in heap!\n");
        for (int i = 0; i < pages; i++) gk.pmm->free_page(mem + i * 4096);
        return;
    }

    mod->magic = dhdr->magic;
    mod->name = dhdr->name;
    mod->type = dhdr->type ? dhdr->type : mod_type;
    mod->module_init = (void* (*)(const KernelAPI*))(mem + dhdr->init_offset);

    gk.all_mods[gk.total_mod_count++] = mod;

    switch (mod->type) {
        case MT_SCHEDULER:
            if (gk.sched_mod_count < 8) gk.sched_mods[gk.sched_mod_count++] = mod;
            break;
        case MT_HEAP:
            if (gk.heap_mod_count < 8) gk.heap_mods[gk.heap_mod_count++] = mod;
            break;
        case MT_PMM:
            if (gk.pmm_mod_count < 8) gk.pmm_mods[gk.pmm_mod_count++] = mod;
            break;
        case MT_FS:
            if (gk.fs_mod_count < 8) gk.fs_mods[gk.fs_mod_count++] = mod;
            break;
        case MT_BLOCKDEV:
            if (gk.block_mod_count < 8) gk.block_mods[gk.block_mod_count++] = mod;
            break;
        default:
            break;
    }

    kprintf("[MAKEMOD] Registered module '%s' (type %d)!\n", mod->name, mod->type);
    kprintf("Type 'load %s' to hot-swap and activate.\n", mod->name);
}

// 14. EDIT
static void cmd_edit(const char* args) {
    while (args && *args == ' ') args++;
    if (!args || args[0] == '\0') {
        kprintf("Usage: edit <filename>\n");
        return;
    }
    char path[32];
    int i = 0;
    while (args[i] && args[i] != ' ' && i < 31) {
        path[i] = args[i];
        i++;
    }
    path[i] = '\0';
    editor_open(path);
}

void shell_execute(char* line) {
    while (*line == ' ') line++;
    if (*line == '\0') return;

    // Support ./<binary> shortcut
    if (line[0] == '.' && line[1] == '/') {
        cmd_run(line + 2);
        return;
    }

    char* cmd_name = line;
    char* args = nullptr;

    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == ' ') {
            line[i] = '\0';
            args = line + i + 1;
            while (*args == ' ') args++;
            break;
        }
    }
    if (!args) args = (char*)"";

    for (uint32_t i = 0; i < num_commands; i++) {
        if (str_eq(cmd_name, commands[i].name)) {
            commands[i].handler(args); // Function pointer call!
            return;
        }
    }

    kprintf("Unknown command: %s (type 'help' for commands)\n", cmd_name);
}

#define SHELL_HIST_MAX 16
static char shell_history[SHELL_HIST_MAX][128];
static int shell_hist_count = 0;
static int shell_hist_browse = -1;
static char shell_draft[128];

void shell_task() {
    char line[128];
    int pos = 0;
    line[0] = '\0';
    kprintf("Welcome to OSig:EKE!\n");
    kprintf("eke> ");
    while (true) {
        char c = get_key();
        if (c == 0) continue;

        if (c == 3) { // Ctrl+C
            pos = 0;
            line[0] = '\0';
            shell_hist_browse = -1;
            shell_draft[0] = '\0';
            kprintf("^C\neke> ");
            continue;
        }

        // UP Arrow: Previous Command in History
        if (c == KEY_UP) {
            if (shell_hist_count > 0) {
                if (shell_hist_browse == -1) {
                    line[pos] = '\0';
                    str_copy(shell_draft, line);
                    shell_hist_browse = shell_hist_count - 1;
                } else if (shell_hist_browse > 0) {
                    shell_hist_browse--;
                }

                while (pos > 0) {
                    terminal_putchar('\b');
                    pos--;
                }

                const char* hist_cmd = shell_history[shell_hist_browse];
                for (int i = 0; hist_cmd[i] != '\0' && pos < 127; i++) {
                    line[pos++] = hist_cmd[i];
                    terminal_putchar(hist_cmd[i]);
                }
                line[pos] = '\0';
            }
            continue;
        }

        // DOWN Arrow: Next Command in History
        if (c == KEY_DOWN) {
            if (shell_hist_browse != -1) {
                if (shell_hist_browse < shell_hist_count - 1) {
                    shell_hist_browse++;
                    while (pos > 0) {
                        terminal_putchar('\b');
                        pos--;
                    }
                    const char* hist_cmd = shell_history[shell_hist_browse];
                    for (int i = 0; hist_cmd[i] != '\0' && pos < 127; i++) {
                        line[pos++] = hist_cmd[i];
                        terminal_putchar(hist_cmd[i]);
                    }
                    line[pos] = '\0';
                } else {
                    shell_hist_browse = -1;
                    while (pos > 0) {
                        terminal_putchar('\b');
                        pos--;
                    }
                    for (int i = 0; shell_draft[i] != '\0' && pos < 127; i++) {
                        line[pos++] = shell_draft[i];
                        terminal_putchar(shell_draft[i]);
                    }
                    line[pos] = '\0';
                }
            }
            continue;
        }

        if (c == '\n') {
            line[pos] = '\0';
            kprintf("\n");
            if (pos > 0) {
                // Deduplicate consecutive identical commands
                if (shell_hist_count == 0 || !str_eq(line, shell_history[shell_hist_count - 1])) {
                    if (shell_hist_count < SHELL_HIST_MAX) {
                        str_copy(shell_history[shell_hist_count], line);
                        shell_hist_count++;
                    } else {
                        for (int h = 0; h < SHELL_HIST_MAX - 1; h++) {
                            str_copy(shell_history[h], shell_history[h + 1]);
                        }
                        str_copy(shell_history[SHELL_HIST_MAX - 1], line);
                    }
                }
                shell_hist_browse = -1;
                shell_draft[0] = '\0';
                shell_execute(line);
            }
            pos = 0;
            line[0] = '\0';
            shell_hist_browse = -1;
            shell_draft[0] = '\0';
            kprintf("eke> ");
            continue;
        }

        if (c == '\b') {
            if (pos > 0) {
                pos--;
                line[pos] = '\0';
                terminal_putchar('\b');
            }
            continue;
        }

        // Printable ASCII characters
        if ((unsigned char)c >= 32 && (unsigned char)c <= 126) {
            if (pos < 127) {
                line[pos++] = c;
                line[pos] = '\0';
                terminal_putchar(c); // Echo to screen!
            }
        }
    }
}
