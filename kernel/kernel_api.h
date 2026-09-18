#ifndef KERNEL_API_H
#define KERNEL_API_H

#include <stdint.h>


struct PMM_API;
struct SCHED_API;
struct HEAP_API;
struct BLOCKDEV_API;
struct FS_API;

struct KernelAPI
{
	void (*log)(const char* format, ...);
	uintptr_t (*alloc_page)(int n);
	void (*free_page)(uintptr_t addr);
};


struct Kernel
{
	struct PMM_API* pmm; 
	struct SCHED_API* sched;
	struct HEAP_API* heap;
	struct HEAP_API* oldheap;

	struct BLOCKDEV_API* disks[8]; // up to 8 registered drives
    uint32_t disk_count;
    struct BLOCKDEV_API* current_disk; // active drive channel
    struct FS_API* fs;

	uint32_t heapcnt;
	uint32_t oldcnt;
	uint32_t flag;
};


typedef struct KernelAPI KernelAPI;

extern "C" struct KernelAPI kapi;

#endif
