#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../interrupts/idt.h"

#define KEY_F1   0x3B
#define KEY_F2   0x3C
#define KEY_F3   0x3D
#define KEY_F4   0x3E
#define KEY_F5   0x3F
#define KEY_F6   0x40
#define KEY_F7   0x41
#define KEY_F8   0x42
#define KEY_F9   0x43
#define KEY_F10  0x44
#define KEY_F11  0x57
#define KEY_F12  0x58

bool is_key_down(uint8_t scancode);
bool is_ctrl_down();
bool is_shift_down();
bool is_alt_down();

void init_keyboard();
void keyboard_handler(struct Registers* regs);
char get_key();

#endif
