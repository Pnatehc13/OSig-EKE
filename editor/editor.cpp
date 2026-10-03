#include "editor.h"
#include "../kernel/tty.h"
#include "../drivers/keyboard.h"
#include "../kernel/kernel_api.h"
#include "../kernel/module_api.h"

extern Kernel gk;

#define MAX_LINES 128
#define MAX_COLS 74
#define VISIBLE_ROWS 23

static char lines[MAX_LINES][MAX_COLS];
static int line_lens[MAX_LINES];
static int total_lines = 1;

static int cur_x = 0;
static int cur_y = 0;
static int scroll_y = 0;

static char filename[64];
static char status_msg[32];
static bool is_dirty = false;

static volatile unsigned short* const VGA = (volatile unsigned short*)0xB8000;

static inline void vga_set(int x, int y, char c, unsigned char color) {
    if (x >= 0 && x < 80 && y >= 0 && y < 25) {
        VGA[y * 80 + x] = (unsigned short)c | ((unsigned short)color << 8);
    }
}

static void vga_print(int x, int y, const char* str, unsigned char color) {
    while (*str && x < 80) {
        vga_set(x++, y, *str++, color);
    }
}

static void vga_fill_row(int y, char c, unsigned char color) {
    for (int x = 0; x < 80; x++) {
        vga_set(x, y, c, color);
    }
}

static void int_to_str(int val, char* buf, int min_digits) {
    char tmp[16];
    int i = 0;
    if (val == 0) tmp[i++] = '0';
    while (val > 0) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i < min_digits) {
        tmp[i++] = ' ';
    }
    int j = 0;
    while (i > 0) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

static void load_file(const char* path) {
    total_lines = 1;
    line_lens[0] = 0;
    lines[0][0] = '\0';
    cur_x = 0;
    cur_y = 0;
    scroll_y = 0;
    is_dirty = false;
    status_msg[0] = '\0';

    int i = 0;
    while (path[i] && i < 63) {
        filename[i] = path[i];
        i++;
    }
    filename[i] = '\0';

    if (!gk.fs) return;

    uint64_t sz = gk.fs->get_size(path);
    if (sz == 0) return;

    char file_buf[8192];
    int n = gk.fs->read_file(path, file_buf, (sz < 8191) ? sz : 8191);
    if (n <= 0) return;
    file_buf[n] = '\0';

    int l = 0;
    int c = 0;
    for (int pos = 0; pos < n && l < MAX_LINES; pos++) {
        char ch = file_buf[pos];
        if (ch == '\r') continue;
        if (ch == '\n') {
            lines[l][c] = '\0';
            line_lens[l] = c;
            l++;
            c = 0;
            continue;
        }
        if (c < MAX_COLS - 1) {
            lines[l][c++] = ch;
        }
    }
    lines[l][c] = '\0';
    line_lens[l] = c;
    total_lines = l + 1;
}

static void save_file() {
    if (!gk.fs) {
        vga_print(50, 24, "[No FS Mounted!]", 0x4F);
        return;
    }

    char out_buf[8192];
    int out_pos = 0;

    for (int l = 0; l < total_lines && out_pos < 8100; l++) {
        for (int c = 0; c < line_lens[l] && out_pos < 8100; c++) {
            out_buf[out_pos++] = lines[l][c];
        }
        if (l < total_lines - 1) {
            out_buf[out_pos++] = '\n';
        }
    }
    out_buf[out_pos] = '\0';

    if (gk.fs->write_file(filename, out_buf, (uint64_t)out_pos) == 0) {
        is_dirty = false;
        int i = 0;
        const char* msg = "[Saved!]";
        while (msg[i]) { status_msg[i] = msg[i]; i++; }
        status_msg[i] = '\0';
    } else {
        int i = 0;
        const char* msg = "[Save Error!]";
        while (msg[i]) { status_msg[i] = msg[i]; i++; }
        status_msg[i] = '\0';
    }
}

static void render_editor() {
    // 1. Top Header Bar (Row 0): Dark Cyan background (0x3F)
    vga_fill_row(0, ' ', 0x3F);
    vga_print(1, 0, "OSig:EKE Text Editor", 0x3F);
    vga_print(26, 0, "| File: ", 0x3E);
    vga_print(34, 0, filename, 0x3F);
    if (is_dirty) {
        vga_print(50, 0, "*MODIFIED*", 0x3E);
    }
    vga_print(63, 0, "^S:Save ^Q:Exit", 0x3B);

    // 2. Text Canvas (Rows 1 to 23 = 23 rows)
    for (int r = 0; r < VISIBLE_ROWS; r++) {
        int file_line = scroll_y + r;
        int screen_y = r + 1;

        if (file_line < total_lines) {
            // Line number: columns 0..3 (cyan on black 0x03)
            char num_buf[8];
            int_to_str(file_line + 1, num_buf, 3);
            vga_print(0, screen_y, num_buf, 0x03);
            vga_set(3, screen_y, ' ', 0x07);
            vga_set(4, screen_y, '|', 0x08);
            vga_set(5, screen_y, ' ', 0x07);

            // Line content: columns 6..79
            int len = line_lens[file_line];
            for (int col = 0; col < 74; col++) {
                char ch = (col < len) ? lines[file_line][col] : ' ';
                vga_set(6 + col, screen_y, ch, 0x0F);
            }
        } else {
            // Blank lines beyond end of file
            vga_set(0, screen_y, '~', 0x03);
            for (int col = 1; col < 80; col++) {
                vga_set(col, screen_y, ' ', 0x07);
            }
        }
    }

    // 3. Status Bar (Row 24): Blue background (0x1F)
    vga_fill_row(24, ' ', 0x1F);
    char stat[64];
    char l_str[8], c_str[8], tot_str[8];
    int_to_str(cur_y + 1, l_str, 1);
    int_to_str(cur_x + 1, c_str, 1);
    int_to_str(total_lines, tot_str, 1);

    vga_print(1, 24, "Line ", 0x1F);
    vga_print(6, 24, l_str, 0x1E);
    vga_print(10, 24, "/", 0x1F);
    vga_print(11, 24, tot_str, 0x1E);
    vga_print(16, 24, "Col ", 0x1F);
    vga_print(20, 24, c_str, 0x1E);

    if (status_msg[0] != '\0') {
        vga_print(35, 24, status_msg, 0x1A);
    }

    // 4. Update hardware cursor
    int screen_cursor_x = 6 + cur_x;
    int screen_cursor_y = (cur_y - scroll_y) + 1;
    update_cursor(screen_cursor_x, screen_cursor_y);
}

void editor_open(const char* filepath) {
    if (!filepath || filepath[0] == '\0') {
        kprintf("Usage: edit <filename>\n");
        return;
    }

    load_file(filepath);

    while (true) {
        // Adjust vertical scrolling if cursor moved off-screen
        if (cur_y < scroll_y) {
            scroll_y = cur_y;
        } else if (cur_y >= scroll_y + VISIBLE_ROWS) {
            scroll_y = cur_y - VISIBLE_ROWS + 1;
        }

        render_editor();

        char c = get_key();
        if (c == 0) continue;

        // Clear transient status message on new keypress
        if (status_msg[0] != '\0') {
            status_msg[0] = '\0';
        }

        // Ctrl+Q: Exit
        if (c == 17) {
            terminal_initialize();
            kprintf("Exited editor.\n");
            return;
        }

        // Ctrl+S: Save
        if (c == 19) {
            save_file();
            continue;
        }

        // Arrow Keys
        if (c == KEY_UP) {
            if (cur_y > 0) {
                cur_y--;
                if (cur_x > line_lens[cur_y]) cur_x = line_lens[cur_y];
            }
            continue;
        }
        if (c == KEY_DOWN) {
            if (cur_y < total_lines - 1) {
                cur_y++;
                if (cur_x > line_lens[cur_y]) cur_x = line_lens[cur_y];
            }
            continue;
        }
        if (c == KEY_LEFT) {
            if (cur_x > 0) cur_x--;
            else if (cur_y > 0) {
                cur_y--;
                cur_x = line_lens[cur_y];
            }
            continue;
        }
        if (c == KEY_RIGHT) {
            if (cur_x < line_lens[cur_y]) cur_x++;
            else if (cur_y < total_lines - 1) {
                cur_y++;
                cur_x = 0;
            }
            continue;
        }

        // Backspace
        if (c == '\b') {
            if (cur_x > 0) {
                // Delete character at cur_x - 1
                for (int i = cur_x - 1; i < line_lens[cur_y]; i++) {
                    lines[cur_y][i] = lines[cur_y][i + 1];
                }
                line_lens[cur_y]--;
                cur_x--;
                is_dirty = true;
            } else if (cur_x == 0 && cur_y > 0) {
                // Merge current line into previous line
                int prev_len = line_lens[cur_y - 1];
                int this_len = line_lens[cur_y];
                if (prev_len + this_len < MAX_COLS - 1) {
                    for (int i = 0; i < this_len; i++) {
                        lines[cur_y - 1][prev_len + i] = lines[cur_y][i];
                    }
                    lines[cur_y - 1][prev_len + this_len] = '\0';
                    line_lens[cur_y - 1] += this_len;

                    // Shift lines below up
                    for (int l = cur_y; l < total_lines - 1; l++) {
                        for (int col = 0; col <= line_lens[l + 1]; col++) {
                            lines[l][col] = lines[l + 1][col];
                        }
                        line_lens[l] = line_lens[l + 1];
                    }
                    total_lines--;
                    cur_y--;
                    cur_x = prev_len;
                    is_dirty = true;
                }
            }
            continue;
        }

        // Enter: Split line
        if (c == '\n') {
            if (total_lines < MAX_LINES) {
                // Shift lower lines down by 1
                for (int l = total_lines; l > cur_y + 1; l--) {
                    for (int col = 0; col <= line_lens[l - 1]; col++) {
                        lines[l][col] = lines[l - 1][col];
                    }
                    line_lens[l] = line_lens[l - 1];
                }

                // Copy split text to new line
                int rem_len = line_lens[cur_y] - cur_x;
                for (int i = 0; i < rem_len; i++) {
                    lines[cur_y + 1][i] = lines[cur_y][cur_x + i];
                }
                lines[cur_y + 1][rem_len] = '\0';
                line_lens[cur_y + 1] = rem_len;

                // Truncate current line
                lines[cur_y][cur_x] = '\0';
                line_lens[cur_y] = cur_x;

                total_lines++;
                cur_y++;
                cur_x = 0;
                is_dirty = true;
            }
            continue;
        }

        // Tab: Insert 4 spaces
        if (c == '\t') {
            for (int s = 0; s < 4 && line_lens[cur_y] < MAX_COLS - 1; s++) {
                for (int i = line_lens[cur_y]; i >= cur_x; i--) {
                    lines[cur_y][i + 1] = lines[cur_y][i];
                }
                lines[cur_y][cur_x] = ' ';
                line_lens[cur_y]++;
                cur_x++;
                is_dirty = true;
            }
            continue;
        }

        // Printable Characters (ASCII 32 to 126)
        if (c >= 32 && c <= 126) {
            if (line_lens[cur_y] < MAX_COLS - 1) {
                for (int i = line_lens[cur_y]; i >= cur_x; i--) {
                    lines[cur_y][i + 1] = lines[cur_y][i];
                }
                lines[cur_y][cur_x] = c;
                line_lens[cur_y]++;
                cur_x++;
                is_dirty = true;
            }
        }
    }
}
