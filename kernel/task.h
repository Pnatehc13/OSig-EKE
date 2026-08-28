#ifndef TASK_H
#define TASK_H

#include "process_api.h"
#include "../interrupts/idt.h"

void init_tasks();

Process* create_task(void (*entry_point)());

struct Registers* schedule_next_task(struct Registers* regs);

#endif
