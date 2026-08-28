#include "task.h"
#include <stdint.h>
#include "process_api.h"
#include "../interrupts/idt.h"
#include "module_api.h"
#include "kernel_api.h"
#include "tty.h"
#include "../memory/vmm.h"
#include "../memory/pmm.h"

extern Kernel gk;
extern uint32_t page_dir[1024]; 
Process kernel_task; 
Process* curr_task = 0;
static int next_pid = 0;

void init_tasks()
{
	kernel_task.pid = 0;
	kernel_task.page_dir=(uintptr_t*)page_dir;
	kernel_task.state = 2;
	curr_task = &kernel_task;
	if(gk.sched)gk.sched->add_task(&kernel_task);
}


Process* create_task(void (*entry_point)())
{
	Process* p = (Process*)alloc_page(1);
	p->pid = ++next_pid;
	p->vnext = 0x40000000;

	gen_vm(p);
	uintptr_t stack = alloc_page(1);
	
	Registers* r = (Registers*)(stack+4096-sizeof(Registers));

	r->ds = 0x10;
	r->edi = 0;
	r->esi = 0;
	r->ebp = 0;
	r->esp = 0;
	r->ebx = 0;
	r->eax = 0;
	r->ecx = 0;
	r->edx = 0;

	r->int_no = 32;
	r->err_code = 0;
	r->eip = (uint32_t) entry_point;
	r->cs = 0x08;
	r->eflags = 0x202;

	p->reg = r;

	p->state = 1;
	if(gk.sched) gk.sched->add_task(p);
	return p;
	
}


struct Registers* schedule_next_task(struct Registers* regs)
{
	if(!gk.sched)return regs;
	curr_task->reg = regs;
	
    Process* next = gk.sched->pick_next(curr_task);
  	curr_task = next;                               
	asm volatile("mov %0, %%cr3" : : "r"(next->page_dir));
	return next->reg;
}


