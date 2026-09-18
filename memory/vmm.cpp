#include "vmm.h"
#include <stdint.h>
#include "pmm.h"

void init_vmm()
{
    // 1GB identity-mapping is already initialized and active from boot.s!
}

// 4-level 64-bit page mapping helper (PML4 -> PDPT -> PD -> PT)
void vmm_map_page(uintptr_t virt, uintptr_t phys, uint64_t flags)
{
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t* pml4 = (uint64_t*)(cr3 & 0x000FFFFFFFFFF000ULL);

    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t* pdpt;
    if ((pml4[pml4_idx] & PTE_P) == 0) {
        pdpt = (uint64_t*)alloc_page(1);
        for (int i = 0; i < 512; i++) pdpt[i] = 0;
        pml4[pml4_idx] = ((uintptr_t)pdpt) | PTE_P | PTE_W;
    } else {
        pdpt = (uint64_t*)(pml4[pml4_idx] & 0x000FFFFFFFFFF000ULL);
    }

    uint64_t* pd;
    if ((pdpt[pdpt_idx] & PTE_P) == 0) {
        pd = (uint64_t*)alloc_page(1);
        for (int i = 0; i < 512; i++) pd[i] = 0;
        pdpt[pdpt_idx] = ((uintptr_t)pd) | PTE_P | PTE_W;
    } else {
        pd = (uint64_t*)(pdpt[pdpt_idx] & 0x000FFFFFFFFFF000ULL);
    }

    uint64_t* pt;
    if ((pd[pd_idx] & PTE_P) == 0) {
        pt = (uint64_t*)alloc_page(1);
        for (int i = 0; i < 512; i++) pt[i] = 0;
        pd[pd_idx] = ((uintptr_t)pt) | PTE_P | PTE_W;
    } else {
        pt = (uint64_t*)(pd[pd_idx] & 0x000FFFFFFFFFF000ULL);
    }

    pt[pt_idx] = (phys & 0x000FFFFFFFFFF000ULL) | flags | PTE_P;

    // Invalidate the CPU's TLB cache for this virtual address
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}
