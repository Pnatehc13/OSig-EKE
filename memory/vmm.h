#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include "../kernel/process_api.h"
#define PTE_P  (1ULL << 0) // Present
#define PTE_W  (1ULL << 1) // Writable
#define PTE_U  (1ULL << 2) // User accessible


void init_vmm();


void vmm_map_page(uintptr_t virt, uintptr_t phys, uint64_t flags);

#endif
