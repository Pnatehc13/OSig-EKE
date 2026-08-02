#include "tty.h"
#include "../memory/pmm.h"
#include "kernel_api.h"
#include "module_api.h"



KernelAPI kapi;
struct Kernel gk;


extern "C" void kernel_main() {
    // Initialize terminal screen (clears screen, sets cyan text)
    terminal_initialize();
    
	
    init_pmm();

    kapi.log = kprintf;
	kapi.alloc_page = alloc_page;
	kapi.free_page = free_page;

	kprintf("[KERNEL] Substrate booted. Loading Module...\n\n");


	 kprintf("[DEBUG] Module Start: %x | Module End: %x\n", &_module_start, &_module_end);
	for(ModuleHeader* t= &_module_start;t< &_module_end;++t)
	{
		kprintf("[KERNEL] Got into forloop\n");
		if(t->type==MT_PMM)
		{
			kprintf("[KERNEL] Got into a module\n");
			gk.pmm = (PMM_API*) t->module_init(&kapi);
			uintptr_t p1 = gk.pmm->alloc_page(1);
			kprintf("[KERNEL] Module allocated page at: %x\n", p1);
		}
	}

}
