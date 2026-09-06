#ifndef HEAP_H
#define HEAP_H

#include "../kernel/kernel_api.h"
#include "../kernel/module_api.h"
#include "../kernel/process_api.h"
#include "../interrupts/idt.h"
#include <stdint.h>

void init_heap();

void* halloc(uint32_t size);

void hfree(void* addr);

#endif
