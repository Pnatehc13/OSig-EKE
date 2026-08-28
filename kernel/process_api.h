#ifndef PROCESS_API_H
#define PROCESS_API_H
#include <stdint.h>
#include "../interrupts/idt.h"

typedef struct Process
{
	uint32_t pid;
	uintptr_t* page_dir;
	//make a local array of sorts which stores the info needed for allocator 
	uintptr_t vnext;   
	struct Registers* reg;
	uint32_t state;
	void* sched_data;
}Process;

#endif
