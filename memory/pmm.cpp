#include "pmm.h"
#include <stdint.h>
#include <stdbool.h>

extern "C" char _kernel_end;

// 16,384 * 64 = 1,048,576 pages = 4GB of physical RAM
#define BM_SIZE 16384
#define MAX_PAGES (BM_SIZE * 64)

uint64_t bitmap[BM_SIZE];
uint64_t last_free_page = 0;

void init_pmm()
{
    uintptr_t kernel_end_addr = (uintptr_t)&_kernel_end;
    last_free_page = (kernel_end_addr + 4095ULL) / 4096ULL;
    uint64_t start_array_idx = last_free_page / 64;
    
    for (uint64_t i = 0; i < BM_SIZE; i++)
    {
        if (i < start_array_idx) bitmap[i] = 0xFFFFFFFFFFFFFFFFULL;
        else bitmap[i] = 0;
    }
}

static inline void setbit(uint64_t ind, int off)
{
    bitmap[ind] |= (1ULL << off);     
}

static inline int getbit(uint64_t ind, int off)
{
    return (bitmap[ind] >> off) & 1ULL;
}

static inline void freebit(uint64_t ind, int off)
{
    bitmap[ind] &= ~(1ULL << off); 
}

uintptr_t alloc_page(int n)
{
    if (n <= 0) return 0;

    uint64_t vi = last_free_page / 64;
    uint64_t vo = last_free_page % 64;
    int i = 0;

    while (1)
    {
        if (i >= n) break;
        uint64_t c_page = (vi * 64 + vo) + i;

        // Out-of-memory guard:
        if (c_page >= MAX_PAGES) return 0;

        uint64_t c_idx = c_page / 64;
        int c_off = c_page % 64;

        if (getbit(c_idx, c_off) == 0)
        {
            i++;
        }
        else
        {
            uint64_t p = c_page + 1;
            vi = p / 64;
            vo = p % 64;
            i = 0;
        }
    }

    for (i = 0; i < n; i++)
    {
        uint64_t p = (vi * 64 + vo) + i;
        setbit(p / 64, p % 64);
    }

    last_free_page = (vi * 64 + vo) + n;

    // 64-bit unsigned multiplication:
    return (uintptr_t)(vi * 64 + vo) * 4096ULL;
}

extern "C" void free_page(uintptr_t addr)
{
    uint64_t p = addr / 4096ULL;
    if (p >= MAX_PAGES) return;

    freebit(p / 64, p % 64);
    if (p < last_free_page)
    {
        last_free_page = p;
    }
}
