#ifndef MODULE_API_H
#define MODULE_API_H


#include <stdint.h>
#include "kernel_api.h"

#define MAGICNUM 0x454B4531

enum ModuleType
{
	MT_SCHEDULER = 1,
	MT_HEAP = 2,
	MT_PMM = 3,
	MT_IPC = 4
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


typedef void* (*module_init_t)(const KernelAPI* api);
#endif
