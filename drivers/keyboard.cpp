#include "keyboard.h"
#include "../interrupts/idt.h"
#include "../kernel/tty.h"
#include <stdint.h>

static inline uint8_t inb(uint16_t port)
{
	uint8_t ret;
	asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}


uint8_t scancode;

char scancode_map[128] = {
        0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
      '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',  0,
      '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
    };

void keyboard_handler(struct Registers* reg)
{
	if(reg->int_no == 33)
	{
		scancode = inb(0x60);
		if(scancode < 0x80)
		{
			kprintf("%c\n",scancode_map[scancode]);
		}
	}
}


