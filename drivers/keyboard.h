#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../interrupts/idt.h"

void init_keyboard();
void keyboard_handler(struct Registers* regs);


#endif
