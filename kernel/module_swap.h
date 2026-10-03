#ifndef MODULE_SWAP_H
#define MODULE_SWAP_H

#include "module_api.h"

bool swap_scheduler(struct ModuleHeader* mod);
bool swap_heap(struct ModuleHeader* mod);
bool swap_pmm(struct ModuleHeader* mod);
bool swap_fs(struct ModuleHeader* mod);
bool swap_blockdev(struct ModuleHeader* mod);

bool swap_module(struct ModuleHeader* mod);

#endif
