#include "shell.h"
#include "tty.h"
#include "../drivers/keyboard.h"
#include "kernel_api.h"


extern struct Kernel gk;

bool str_eq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return false;
        a++; b++;
    }
    return *a == *b;
}

void shell_execute(char* line) {
    if (str_eq(line, "help")) 
    {
        kprintf("Commands: help, clear, echo, mem\n");
    }
    else if (str_eq(line, "clear")) 
    {
        terminal_initialize();
    } 
    else if (str_eq(line, "mem")) 
    {
        kprintf("Heap Allocations: %d\n", gk.heapcnt);
    } 
    else if (line[0] == 'e' && line[1] == 'c' && line[2] == 'h' && line[3] == 'o' && line[4] == ' ') 
    {
        kprintf("%s\n", line + 5);
    } 
    else 
    {
        kprintf("Unknown command: %s\n", line);
    }
}

void shell_task()
{
	char line[128];
	int pos = 0;
	kprintf("Welcome to OSig:EKE!\n");
    kprintf("eke> ");
	while(true)
	{
		char c = get_key();
		if(c==0)continue;

		if (c == 3) 
		{
			pos = 0;
            kprintf("^C\neke> ");
            continue;	            
        }
        if (c == '\n') 
        {
            line[pos] = '\0';
            kprintf("\n");
            if (pos > 0) 
            {
                shell_execute(line);
            }
            pos = 0;
            kprintf("eke> ");
        	continue;
        }
        if (c == '\b') 
        {
            if (pos > 0) {
                pos--;
                terminal_putchar('\b');
            }
            continue;
        }
        if (pos < 127) 
        {
            line[pos++] = c;
            terminal_putchar(c); // Echo to screen!
        }
        
	}
}
