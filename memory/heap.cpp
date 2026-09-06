#include "heap.h"
#include "../kernel/kernel_api.h"
#include "../kernel/module_api.h"
#include "../kernel/process_api.h"
#include "../interrupts/idt.h"
#include <stdint.h>

extern Kernel gk;

void init_heap()
{
	gk.heapcnt = 0;
	gk.oldcnt = 0;
	gk.flag = 0;
}

void* halloc(uint32_t size)
{	
	gk.heapcnt++;
	return gk.heap->kmalloc(size);
}

void hfree(void* addr)
{
	bool f = false;
	if((gk.flag&1) == 1)
	{	
	   f = gk.oldheap->kfree(addr);
	   if(f) gk.oldcnt--;
	}
	if(!f && gk.heap)
	{
		if(gk.heap->kfree(addr))
		{
			gk.heapcnt--;
		}
	}
	if((gk.flag&1)==1 && gk.oldcnt <= 0){
		gk.oldheap = nullptr;
	}
}

