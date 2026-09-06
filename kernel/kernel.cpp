#include "tty.h"
#include "../memory/pmm.h"
#include "kernel_api.h"
#include "module_api.h"
#include "../interrupts/idt.h"
#include "../memory/vmm.h"
#include "process_api.h"
#include "task.h"
#include "../memory/heap.h"
#include "shell.h"

KernelAPI kapi;
struct Kernel gk;

void task1() {
    while (true) {
        char* buf = (char*)halloc(32);
        if (buf) {
            buf[0] = 'T'; buf[1] = '1'; buf[2] = ' '; buf[3] = 'O'; buf[4] = 'K'; buf[5] = '\0';
            kprintf("[TASK 1] Halloc at %x: %s\n", (uint32_t)buf, buf);
            hfree(buf);
        }
        for (volatile int i = 0; i < 30000000; i++);
    }
}

void task2() {
    while (true) {
        char* buf = (char*)halloc(32);
        if (buf) {
            buf[0] = 'T'; buf[1] = '2'; buf[2] = ' '; buf[3] = 'O'; buf[4] = 'K'; buf[5] = '\0';
            kprintf("[TASK 2] Halloc at %x: %s\n", (uint32_t)buf, buf);
            hfree(buf);
        }
        for (volatile int i = 0; i < 30000000; i++);
    }
}

extern "C" void kernel_main() {
    terminal_initialize();
    
    init_pmm();
    init_vmm();
    init_idt();
    init_heap();
    kapi.log = kprintf;
    kapi.alloc_page = alloc_page;
    kapi.free_page = free_page;

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
			if (mod->type == MT_HEAP) {
			    gk.heap = (struct HEAP_API*)mod->module_init(&kapi);
			    kprintf("[MODULE] Loaded: %s\n", mod->name);
			}
        }
        mod++;
	}

    init_heap();

    init_tasks();
    create_task(shell_task);

    while (true) {
        asm volatile("hlt");
    }

   
}
