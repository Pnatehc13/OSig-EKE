#include "tty.h"

static const unsigned int VGA_WIDTH = 80;
static const unsigned int VGA_HEIGHT = 25;
static volatile unsigned short* const VGA_BUFFER = (volatile unsigned short*)0xB8000;

static unsigned int terminal_row;
static unsigned int terminal_column;
static unsigned char terminal_color;

static unsigned short vga_entry(char c, unsigned char color) {
    return (unsigned short)c | ((unsigned short)color << 8);
}

static inline void outb(unsigned short port, unsigned char val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void update_hardware_cursor() {
    unsigned short pos = terminal_row * VGA_WIDTH + terminal_column;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

#define SCROLLBACK_LINES 256

static unsigned short live_screen[VGA_HEIGHT][VGA_WIDTH];
static unsigned short history[SCROLLBACK_LINES][VGA_WIDTH];
static int history_count = 0;
static int history_head = 0;
static int scroll_offset = 0; // 0 = live screen, > 0 = history

static void terminal_render() {
    if (scroll_offset == 0) {
        for (unsigned int y = 0; y < VGA_HEIGHT; ++y) {
            for (unsigned int x = 0; x < VGA_WIDTH; ++x) {
                VGA_BUFFER[y * VGA_WIDTH + x] = live_screen[y][x];
            }
        }
        update_hardware_cursor();
    } else {
        int start_line = history_count - scroll_offset;
        for (unsigned int y = 0; y < VGA_HEIGHT; ++y) {
            int line_idx = start_line + y;
            if (line_idx < 0) {
                for (unsigned int x = 0; x < VGA_WIDTH; ++x) {
                    VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
                }
            } else if (line_idx < history_count) {
                int hist_slot;
                if (history_count < SCROLLBACK_LINES) {
                    hist_slot = line_idx;
                } else {
                    hist_slot = (history_head + line_idx) % SCROLLBACK_LINES;
                }
                for (unsigned int x = 0; x < VGA_WIDTH; ++x) {
                    VGA_BUFFER[y * VGA_WIDTH + x] = history[hist_slot][x];
                }
            } else {
                int live_y = line_idx - history_count;
                if (live_y < (int)VGA_HEIGHT) {
                    for (unsigned int x = 0; x < VGA_WIDTH; ++x) {
                        VGA_BUFFER[y * VGA_WIDTH + x] = live_screen[live_y][x];
                    }
                }
            }
        }
        // Move cursor off-screen during scrollback
        outb(0x3D4, 0x0F);
        outb(0x3D5, (unsigned char)(0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (unsigned char)(0xFF));
    }
}

static void terminal_scroll() {
    // 1. Save Row 0 into scrollback history!
    for (unsigned int x = 0; x < VGA_WIDTH; ++x) {
        history[history_head][x] = live_screen[0][x];
    }
    history_head = (history_head + 1) % SCROLLBACK_LINES;
    if (history_count < SCROLLBACK_LINES) {
        history_count++;
    }

    // 2. Shift live screen up by 1
    for (unsigned int y = 0; y < VGA_HEIGHT - 1; ++y) {
        for (unsigned int x = 0; x < VGA_WIDTH; ++x) {
            live_screen[y][x] = live_screen[y + 1][x];
        }
    }
    // Clear bottom row
    for (unsigned int x = 0; x < VGA_WIDTH; ++x) {
        live_screen[VGA_HEIGHT - 1][x] = vga_entry(' ', terminal_color);
    }
    terminal_row = VGA_HEIGHT - 1;

    // 3. Render
    terminal_render();
}

extern "C" void terminal_initialize() {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = (VGA_COLOR_BLACK << 4) | VGA_COLOR_LIGHT_CYAN;
    scroll_offset = 0;
    
    for (unsigned int y = 0; y < VGA_HEIGHT; ++y) {
        for (unsigned int x = 0; x < VGA_WIDTH; ++x) {
            live_screen[y][x] = vga_entry(' ', terminal_color);
            VGA_BUFFER[y * VGA_WIDTH + x] = live_screen[y][x];
        }
    }
    update_hardware_cursor();
}

extern "C" void terminal_setcolor(unsigned char color) {
    terminal_color = color;
}

extern "C" void terminal_putchar(char c) {
    if (scroll_offset > 0) {
        scroll_offset = 0;
        terminal_render();
    }

	if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            live_screen[terminal_row][terminal_column] = vga_entry(' ', terminal_color);
            VGA_BUFFER[terminal_row * VGA_WIDTH + terminal_column] = live_screen[terminal_row][terminal_column];
            update_hardware_cursor();
        }
        return;
    }
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        } else if (scroll_offset == 0) {
            update_hardware_cursor();
        }
        return;
    }

    live_screen[terminal_row][terminal_column] = vga_entry(c, terminal_color);
    VGA_BUFFER[terminal_row * VGA_WIDTH + terminal_column] = live_screen[terminal_row][terminal_column];
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        }
    }
    if (scroll_offset == 0) {
        update_hardware_cursor();
    }
}

extern "C" void terminal_scroll_up(int lines) {
    if (history_count == 0) return;
    scroll_offset += lines;
    if (scroll_offset > history_count) {
        scroll_offset = history_count;
    }
    terminal_render();
}

extern "C" void terminal_scroll_down(int lines) {
    if (scroll_offset == 0) return;
    scroll_offset -= lines;
    if (scroll_offset < 0) {
        scroll_offset = 0;
    }
    terminal_render();
}

extern "C" void terminal_scroll_reset() {
    if (scroll_offset != 0) {
        scroll_offset = 0;
        terminal_render();
    }
}

extern "C" void terminal_writestring(const char* data) {
    for (unsigned int i = 0; data[i] != '\0'; ++i) {
        terminal_putchar(data[i]);
    }
}

static void print_dec(unsigned int n) {
    if (n == 0) {
        terminal_putchar('0');
        return;
    }
    char buf[32];
    int i = 0;
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (--i >= 0) {
        terminal_putchar(buf[i]);
    }
}

static void print_hex(unsigned int n) {
    terminal_writestring("0x");
    if (n == 0) {
        terminal_putchar('0');
        return;
    }
    char buf[32];
    const char* hex_chars = "0123456789ABCDEF";
    int i = 0;
    while (n > 0) {
        buf[i++] = hex_chars[n % 16];
        n /= 16;
    }
    while (--i >= 0) {
        terminal_putchar(buf[i]);
    }
}

extern "C" void kprintf(const char* format, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, format);

    for (int i = 0; format[i] != '\0'; ++i) {
        if (format[i] == '%' && format[i + 1] != '\0') {
            ++i;
            if (format[i] == 's') {
                const char* str = __builtin_va_arg(args, const char*);
                terminal_writestring(str ? str : "(null)");
            } else if (format[i] == 'd') {
                int val = __builtin_va_arg(args, int);
                if (val < 0) {
                    terminal_putchar('-');
                    val = -val;
                }
                print_dec((unsigned int)val);
            } else if (format[i] == 'u') {
                unsigned int val = __builtin_va_arg(args, unsigned int);
                print_dec(val);
            } else if (format[i] == 'x' || format[i] == 'p') {
                unsigned int val = __builtin_va_arg(args, unsigned int);
                print_hex(val);
            } else if (format[i] == 'c') {
                char c = (char)__builtin_va_arg(args, int);
                terminal_putchar(c);
            } else if (format[i] == '%') {
                terminal_putchar('%');
            }
        } else {
            terminal_putchar(format[i]);
        }
    }

    __builtin_va_end(args);
}
