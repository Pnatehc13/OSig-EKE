#include "idt.h"
#include "../kernel/tty.h"
#include "../drivers/keyboard.h"
#include "../kernel/task.h"


struct IDTEntry idt[256];
struct IDTPtr idtptr;

void set_idt_gate(int n,uint32_t handler , uint8_t flags)
{
	idt[n].isr_low = handler&((1<<16)-1);
	idt[n].isr_high = handler>>16;
	idt[n].attributes = flags;
	idt[n].kernel_cs = 0x08;
	idt[n].reserved = 0;
}


extern "C" void* isr_stub_table[256];
extern "C" void idtload(uint32_t addr);

static inline void outb(uint16_t port, uint8_t val)
{
	asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void pic_remap()
{
	outb(0x20, 0x11); outb(0xA0, 0x11); // ICW1: Init PICs
	outb(0x21, 0x20); outb(0xA1, 0x28); // ICW2: Remap IRQ 0..7 -> 32, IRQ 8..15 -> 40
	outb(0x21, 0x04); outb(0xA1, 0x02); // ICW3: Cascade
	outb(0x21, 0x01); outb(0xA1, 0x01); // ICW4: 8086 mode
	outb(0x21, 0x00); outb(0xA1, 0x00); // Unmask IRQs
}

void init_idt()
{
	idtptr.limit = (sizeof(IDTEntry)*256) - 1;
	idtptr.base = (uint32_t)&idt[0];

	for(int i=0;i<256;i++)
	{
		uint8_t f = (i==0x80)?0xEE:0x8E;
		set_idt_gate(i,(uint32_t)isr_stub_table[i],f);
	}
	idtload((uint32_t)&idtptr);
	pic_remap();
	asm volatile("sti");
}



extern "C" Registers* isr_handler(Registers * regs)
{
	if(regs->int_no >= 32 && regs->int_no <= 47)
	{
		if(regs->int_no>=40)outb(0xA0,0x20);
		outb(0x20,0x20);
	}
	if (regs->int_no == 32) 
	{
		return schedule_next_task(regs);
	}
	else if(regs->int_no == 33)
	{
		keyboard_handler(regs);
	}
	else 
	{
		 kprintf("[INTERRUPT] Fired Interrupt: %d | Error Code: %d | EIP: %x\n", regs->int_no, regs->err_code, regs->eip);
	}
	return regs;
}

