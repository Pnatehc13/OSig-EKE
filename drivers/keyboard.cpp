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

static char key_buffer[128];
static int write_idx = 0;
static int read_idx = 0;

bool shift_p = false;
bool ctrl_p = false;

static bool key_state[256];

bool is_key_down(uint8_t scancode) {
	return key_state[scancode];
}

uint8_t scancode;

char scancode_map[128] = {
        0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
      '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',  0,
      '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
    };

char scancode_map_shift[128] = {
        0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
      '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
        0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',  0,
      '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0, '*',   0, ' '
    };

void keyboard_handler(struct Registers* reg)
{
	if(reg->int_no == 33)
	{
		scancode = inb(0x60);
		if (scancode >= 0x80)
        {
            key_state[scancode - 0x80] = false;
            return;
        }
        key_state[scancode] = true;
		if(scancode < 0x80)
		{
			char c = 0;
			if (is_ctrl_down()) {
			    char base = scancode_map[scancode];
			    if (base >= 'a' && base <= 'z') 
			    {
			        c = base - 'a' + 1; 
				}
			} 
			else if (is_shift_down()) c = scancode_map_shift[scancode];
			else c = scancode_map[scancode];
		
			if(c!=0)
			{
				if ((write_idx + 1) % 128 == read_idx)
				{
				    read_idx = (read_idx + 1) % 128;
				}
				key_buffer[write_idx] = c;
				write_idx = (write_idx + 1) % 128;
			}
		}
	}
}

bool is_ctrl_down() {
    return key_state[0x1D]||key_state[0x3A];
}

bool is_shift_down() {
    return key_state[0x2A] || key_state[0x36]; 
}

bool is_alt_down() {
    return key_state[0x38];
}



char get_key()
{
	if(read_idx != write_idx)
	{
		char c = key_buffer[read_idx];
		read_idx = (read_idx + 1) % 128; 
		return c;
	}
	return 0;
}


