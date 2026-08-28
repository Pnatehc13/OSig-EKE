#include "tty.h"
#include "../memory/pmm.h"
#include "kernel_api.h"
#include "module_api.h"
#include "../interrupts/idt.h"
#include "../memory/vmm.h"
#include "process_api.h"
#include "task.h"

KernelAPI kapi;
struct Kernel gk;

void task1() {
    while (true) {
        kprintf("[TASK 1] Running...\n");
        for (volatile int i = 0; i < 30000000; i++);
    }
}

void task2() {
    while (true) {
        kprintf("[TASK 2] Hello from Task 2!\n");
        for (volatile int i = 0; i < 30000000; i++);
    }
}

extern "C" void kernel_main() {
    terminal_initialize();
    
    init_pmm();
    init_vmm();
    init_idt();


    ModuleHeader* mod = &_module_start;
	while (mod < &_module_end) 
	{
	    if (mod->magic == MAGICNUM) 
	    {
            if (mod->type == MT_SCHEDULER) 
            {
		   		gk.sched = (struct SCHED_API*)mod->module_init(&kapi);
                kprintf("[MODULE] Loaded: %s\n", mod->name);
			}
        }
        mod++;
	}

    init_tasks();
    create_task(task1);
    create_task(task2);

    kprintf("[KERNEL] Multitasking Started!\n");

    while (true) {
        asm volatile("hlt");
    }
}
