#include "shell.h"
#include "tty.h"
#include "../drivers/keyboard.h"
#include "kernel_api.h"
#include "module_api.h"
#include "../storage/spmm.h"

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
static void cmd_disks(const char* args);
static void cmd_drive(const char* args);
static void cmd_ls(const char* args);
static void cmd_cat(const char* args);
static void cmd_write(const char* args);
static void cmd_rm(const char* args);

// The Command Dispatch Table
static const ShellCommand commands[] = {
    {"help",  "Show this help menu",        cmd_help},
    {"clear", "Clear terminal screen",      cmd_clear},
    {"mem",   "Show heap allocations",      cmd_mem},
    {"echo",  "Print text to screen",       cmd_echo},
    {"disks", "List registered drives",     cmd_disks},
    {"drive", "Switch active drive",        cmd_drive},
    {"ls",    "List files on drive",        cmd_ls},
    {"cat",   "Read and display a file",   cmd_cat},
    {"write", "Write text to a file",       cmd_write},
    {"rm",    "Delete a file from drive",  cmd_rm}
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

// 5. DISKS
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

void shell_execute(char* line) {
    while (*line == ' ') line++;
    if (*line == '\0') return;

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

void shell_task() {
    char line[128];
    int pos = 0;
    kprintf("Welcome to OSig:EKE!\n");
    kprintf("eke> ");
    while (true) {
        char c = get_key();
        if (c == 0) continue;

        if (c == 3) { // Ctrl+C
            pos = 0;
            kprintf("^C\neke> ");
            continue;
        }
        if (c == '\n') {
            line[pos] = '\0';
            kprintf("\n");
            if (pos > 0) {
                shell_execute(line);
            }
            pos = 0;
            kprintf("eke> ");
            continue;
        }
        if (c == '\b') {
            if (pos > 0) {
                pos--;
                terminal_putchar('\b');
            }
            continue;
        }
        if (pos < 127) {
            line[pos++] = c;
            terminal_putchar(c); // Echo to screen!
        }
    }
}
