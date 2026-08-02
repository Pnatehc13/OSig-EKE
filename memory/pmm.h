
#ifndef PMM_H
#define PMM_H

#include <stdint.h>

extern "C" {
void init_pmm();
uintptr_t alloc_page(int n);
void free_page(uintptr_t addr);
}

#endif
