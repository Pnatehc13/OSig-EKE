    #include "../../kernel/module_api.h"
   #include "../../kernel/kernel_api.h"
#include <stdint.h>

static uintptr_t heap_base = 0;
static uint32_t heap_offset = 0;
static uint32_t heap_capacity = 4096; // 1 page
static const KernelAPI* kernel_api = 0;

void* bump_kmalloc(uint32_t size) {
    // 4-byte align the allocation size:
    size = (size + 3) & ~3;

    if (heap_offset + size > heap_capacity) {
        // Out of space in current page
        return 0;
    }

    void* ptr = (void*)(heap_base + heap_offset);
    heap_offset += size;
    return ptr;
}

bool bump_kfree(void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    // Check if this pointer belongs to our heap page:
    if (addr >= heap_base && addr < (heap_base + heap_capacity)) {
        return true;
    }
    return false;
}

static struct HEAP_API bump_api = {
    bump_kmalloc,
    bump_kfree
};

void* bump_init(const KernelAPI* api) {
    kernel_api = api;
    // Allocate our initial 4KB physical heap page:
    heap_base = api->alloc_page(1);
    heap_offset = 0;
    return (void*)&bump_api;
}

__attribute__((section(".modules"))) ModuleHeader bump_mod = {
    MAGICNUM,
    "BumpHeapAlloc",
    MT_HEAP,
    bump_init
};
