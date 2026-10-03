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


struct ModuleHeader;

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

	// Module Registry Arrays (categorized by type)
	struct ModuleHeader* sched_mods[8];   uint32_t sched_mod_count;
	struct ModuleHeader* heap_mods[8];    uint32_t heap_mod_count;
	struct ModuleHeader* pmm_mods[8];     uint32_t pmm_mod_count;
	struct ModuleHeader* block_mods[8];   uint32_t block_mod_count;
	struct ModuleHeader* fs_mods[8];      uint32_t fs_mod_count;
	struct ModuleHeader* all_mods[32];    uint32_t total_mod_count;

	uint32_t heapcnt;
	uint32_t oldcnt;
	uint32_t flag;
};


typedef struct KernelAPI KernelAPI;

#ifdef __cplusplus
extern "C" struct KernelAPI kapi;
#else
extern struct KernelAPI kapi;
#endif

#endif
