#ifndef MODULE_API_H
#define MODULE_API_H


#include <stdint.h>
#include "kernel_api.h"
#include "process_api.h"
#define MAGICNUM 0x454B4531

enum ModuleType
{
	MT_SCHEDULER = 1,
	MT_HEAP = 2,
	MT_PMM = 3,
	MT_IPC = 4,
	MT_FS = 5
};

typedef struct ModuleHeader
{
	uint32_t magic;
	const char* name;
	uint32_t type;
	void* (*module_init)(const KernelAPI* api);
}ModuleHeader;


extern "C" ModuleHeader _module_start;
extern "C" ModuleHeader _module_end;

struct PMM_API
{
	uintptr_t (*alloc_page)(int n);
	void (*free_page)(uint64_t addr);
};

struct SCHED_API
{
	void (*add_task)(struct Process* proc);
	struct Process* (*pick_next)(struct Process* current);
};

struct HEAP_API
{
	void* (*kmalloc)(uint32_t size);
	bool (*kfree)(void* ptr);
};

struct FS_API
{
		
};

typedef void* (*module_init_t)(const KernelAPI* api);
#endif
