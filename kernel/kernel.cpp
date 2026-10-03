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
/*
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
*/
extern "C" void kernel_main() {
    terminal_initialize();
    
    init_pmm();
    init_vmm();
    init_idt();
    init_heap();
    kapi.log = kprintf;
    kapi.alloc_page = alloc_page;
    kapi.free_page = free_page;
    
    gk.disk_count = 0;
    gk.current_disk = nullptr;
    gk.fs = nullptr;
    gk.sched = nullptr;
    gk.heap = nullptr;
    gk.pmm = nullptr;
    gk.sched_mod_count = 0;
    gk.heap_mod_count = 0;
    gk.pmm_mod_count = 0;
    gk.block_mod_count = 0;
    gk.fs_mod_count = 0;
    gk.total_mod_count = 0;

    ModuleHeader* mod = &_module_start;
	while (mod < &_module_end) 
	{
	    if (mod->magic == MAGICNUM) 
	    {
            if (gk.total_mod_count < 32) {
                gk.all_mods[gk.total_mod_count++] = mod;
            }

            if (mod->type == MT_SCHEDULER && gk.sched_mod_count < 8) 
            {
                gk.sched_mods[gk.sched_mod_count++] = mod;
                if (!gk.sched) {
		   		    gk.sched = (struct SCHED_API*)mod->module_init(&kapi);
                }
                kprintf("[MODULE] Registered Scheduler: %s\n", mod->name);
			}
			else if (mod->type == MT_HEAP && gk.heap_mod_count < 8) {
                gk.heap_mods[gk.heap_mod_count++] = mod;
                if (!gk.heap) {
			        gk.heap = (struct HEAP_API*)mod->module_init(&kapi);
                }
			    kprintf("[MODULE] Registered Heap: %s\n", mod->name);
			}
            else if (mod->type == MT_PMM && gk.pmm_mod_count < 8) {
                gk.pmm_mods[gk.pmm_mod_count++] = mod;
                if (!gk.pmm) {
                    gk.pmm = (struct PMM_API*)mod->module_init(&kapi);
                }
                kprintf("[MODULE] Registered PMM: %s\n", mod->name);
            }
			else if (mod->type == MT_BLOCKDEV && gk.block_mod_count < 8) {
                gk.block_mods[gk.block_mod_count++] = mod;
	            if (gk.disk_count < 8) {
	                BLOCKDEV_API* dev = (BLOCKDEV_API*)mod->module_init(&kapi);
	                if (dev) {
	                    gk.disks[gk.disk_count++] = dev;
	                    if (!gk.current_disk) gk.current_disk = dev; // First drive becomes active
	                    kprintf("[MODULE] Registered Block Device: %s (%s)\n", mod->name, dev->name);
	                }
	            }
	        }
	        else if (mod->type == MT_FS && gk.fs_mod_count < 8) {
                gk.fs_mods[gk.fs_mod_count++] = mod;
                if (!gk.fs) {
                    gk.fs = (FS_API*)mod->module_init(&kapi);
                }
                kprintf("[MODULE] Registered Filesystem: %s\n", mod->name);
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
