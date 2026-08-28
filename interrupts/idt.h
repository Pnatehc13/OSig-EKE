#ifndef IDT_H
#define IDT_H

#include <stdint.h>


struct IDTEntry
{
	uint16_t isr_low;
	uint16_t kernel_cs;
	uint8_t  reserved;
	uint8_t  attributes;
	uint16_t isr_high;
}__attribute__((packed));

struct IDTPtr
{
	uint16_t limit;
	uint32_t base;
} __attribute__((packed));


struct Registers
{
	uint32_t ds;
	uint32_t edi,esi,ebp,esp,ebx,edx,ecx,eax;
	uint32_t int_no , err_code;
	uint32_t eip,cs,eflags,useresp,ss;
};

void init_idt();
void set_idt_gate(int n,uint32_t handler,uint8_t flags);

#endif
