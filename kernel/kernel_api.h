#ifndef KERNEL_API_H
#define KERNEL_API_H

#include <stdint.h>


struct PMM_API;



struct KernelAPI
{
	void (*log)(const char* format, ...);
	uintptr_t (*alloc_page)(int n);
	void (*free_page)(uintptr_t addr);
};


struct Kernel
{
	struct PMM_API* pmm; 
};


typedef struct KernelAPI KernelAPI;

extern "C" struct KernelAPI kapi;

#endif
