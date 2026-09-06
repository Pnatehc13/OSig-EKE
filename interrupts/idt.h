#ifndef IDT_H
#define IDT_H

#include <stdint.h>


struct IDTEntry
{
	uint16_t isr_low;
	uint16_t kernel_cs;
	uint8_t  ist;
	uint8_t  attributes;
	uint16_t isr_mid;
	uint32_t isr_high;
	uint32_t reserved;
}__attribute__((packed));

struct IDTPtr
{
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));


struct Registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss; // Pushed automatically by 64-bit CPU!
};


void init_idt();
void set_idt_gate(int n,uint64_t handler,uint8_t flags);

#endif
