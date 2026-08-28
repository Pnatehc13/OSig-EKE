#include "./vmm.h"
#include <stdint.h>
#include "pmm.h"
#include "../kernel/process_api.h"

__attribute__((aligned(4096))) uint32_t page_dir[1024];
__attribute__((aligned(4096))) uint32_t Kernel_page_tab[1024];

void init_vmm()
{
	for (int i = 0; i < 1024; i++) 
	{
	        page_dir[i] = 0;
	}
	for(int i=0;i<1024;i++)
	{
		Kernel_page_tab[i] = (4096*i)|PTE_P|PTE_W;
	}
	page_dir[0] = ((uintptr_t)Kernel_page_tab)|PTE_P|PTE_W;

	asm volatile("mov %0, %%cr3" : : "r"(page_dir));
	uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
	        
}

void map_in_pd(Process* pd,uintptr_t virt, uintptr_t phys, uint32_t flags)
{
	uint32_t dir_idx = (virt>>22) & 0x3FF;
	uint32_t tab_idx = (virt>>12) & 0x3FF;	
    
	if((pd->page_dir[dir_idx]&PTE_P) == 0)
	{
		uintptr_t* tp =(uintptr_t *) alloc_page(1);
		for(int i =0;i<1024;i++)
		{
			tp[i] = 0;
		}
		pd->page_dir[dir_idx] =(uint32_t) tp | flags;	
	}
	uintptr_t* pt = (uintptr_t*)(pd->page_dir[dir_idx] & 0xFFFFF000);
	pt[tab_idx] =( phys & 0xFFFFF000)|flags;
} 


uintptr_t vmm_alloc_page(Process* p,uint32_t flags)
{
	p->vnext+=4096;
	uintptr_t phys = alloc_page(1);
	map_in_pd(p,p->vnext,phys,flags);
	return p->vnext;
}

void gen_vm(Process* p)
{
	uintptr_t* vm_pd =(uintptr_t *) alloc_page(1);
	for(int i =0;i<1024;i++)
	{
		vm_pd[i] = 0;
	}
	vm_pd[0] = page_dir[0]; 

	
	p->page_dir = vm_pd;	
}





