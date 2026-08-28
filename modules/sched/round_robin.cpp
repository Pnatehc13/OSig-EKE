#include "../../kernel/module_api.h"
#include "../../kernel/kernel_api.h"
#include "../../kernel/process_api.h"

static Process* ready_queue[32];
static int queue_count = 0;
static int current_idx = 0;


 void rr_add_task(Process* proc) 
{
	if (queue_count < 32) 
    {
	    ready_queue[queue_count++] = proc;
	}
}

Process* rr_pick_next(Process* current)
{
    if (queue_count == 0) return current;
	current_idx = (current_idx + 1) % queue_count;
	return ready_queue[current_idx];
}

 static struct SCHED_API rr_api = {
    rr_add_task,
    rr_pick_next
};

void* rr_init(const KernelAPI* api)
{
	return (void*)&rr_api;
}

__attribute__((section(".modules"))) ModuleHeader rr_mod={
	MAGICNUM,
	"RoundRobinSched",
	MT_SCHEDULER,
	rr_init
};

