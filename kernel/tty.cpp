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

static void terminal_scroll() {
    // Shift all rows up by 1
    for (unsigned int y = 0; y < VGA_HEIGHT - 1; ++y) {
        for (unsigned int x = 0; x < VGA_WIDTH; ++x) {
            VGA_BUFFER[y * VGA_WIDTH + x] = VGA_BUFFER[(y + 1) * VGA_WIDTH + x];
        }
    }
    // Clear the bottom row
    for (unsigned int x = 0; x < VGA_WIDTH; ++x) {
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
    terminal_row = VGA_HEIGHT - 1;
}

extern "C" void terminal_initialize() {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = (VGA_COLOR_BLACK << 4) | VGA_COLOR_LIGHT_CYAN;
    
    for (unsigned int y = 0; y < VGA_HEIGHT; ++y) {
        for (unsigned int x = 0; x < VGA_WIDTH; ++x) {
            VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
        }
    }
}

extern "C" void terminal_setcolor(unsigned char color) {
    terminal_color = color;
}

extern "C" void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        }
        return;
    }

    VGA_BUFFER[terminal_row * VGA_WIDTH + terminal_column] = vga_entry(c, terminal_color);
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        }
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
