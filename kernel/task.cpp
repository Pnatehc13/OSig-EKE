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
Process kernel_task; 
Process* curr_task = 0;
static int next_pid = 0;

void init_tasks()
{
	kernel_task.pid = 0;
	kernel_task.state = 2;
	curr_task = &kernel_task;
	if(gk.sched)gk.sched->add_task(&kernel_task);
}


Process* create_task(void (*entry_point)())
{
    Process* p = (Process*)alloc_page(1);
    p->pid = ++next_pid;
    p->vnext = 0x40000000;

    uintptr_t stack = alloc_page(1);

    Registers* r = (Registers*)(stack + 4096 - sizeof(Registers));

    r->r15 = 0; r->r14 = 0; r->r13 = 0; r->r12 = 0;
    r->r11 = 0; r->r10 = 0; r->r9  = 0; r->r8  = 0;
    r->rbp = 0; r->rdi = 0; r->rsi = 0; r->rdx = 0;
    r->rcx = 0; r->rbx = 0; r->rax = 0;

    r->int_no = 32;
    r->err_code = 0;
    r->rip = (uint64_t)entry_point;
    r->cs = 0x08;                     // 64-bit Code Segment
    r->rflags = 0x202;		  // Interrupts enabled
    r->rsp = (uint64_t)(stack + 4096 - sizeof(Registers));
    r->ss = 0x10;                     // 64-bit Data Segment

    p->reg = r;
    p->state = 1;
    if (gk.sched) gk.sched->add_task(p);
    return p;
}



struct Registers* schedule_next_task(struct Registers* regs)
{
	if(!gk.sched)return regs;
	curr_task->reg = regs;
	
    Process* next = gk.sched->pick_next(curr_task);
  	curr_task = next;                               
	return next->reg;
}


