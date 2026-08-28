#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include "../kernel/process_api.h"
#define PTE_P 0x01
#define PTE_W 0x02
#define PTE_U 0x04


void init_vmm();
void map_in_pd(Process* pd,uintptr_t virt, uintptr_t phys, uint32_t flags);
uintptr_t vmm_alloc_page(Process* p, uint32_t flags);
void gen_vm(Process* p);


#endif
