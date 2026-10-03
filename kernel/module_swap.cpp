#include "module_swap.h"
#include "kernel_api.h"
#include "tty.h"
#include "task.h"

extern struct Kernel gk;
extern "C" struct KernelAPI kapi;

bool swap_scheduler(struct ModuleHeader* mod) {
    if (!mod || !mod->module_init) return false;

    // 1. Pause timer interrupts so no context switch occurs mid-swap
    asm volatile("cli");

    struct SCHED_API* new_sched = (struct SCHED_API*)mod->module_init(&kapi);
    if (!new_sched) {
        asm volatile("sti");
        kprintf("[SWAP] Error: Failed to init scheduler '%s'\n", mod->name);
        return false;
    }

    // 2. Walk master list: prune dead tasks (state == 0), feed active tasks
    Process* curr = listhead;
    Process* prev = nullptr;
    int count = 0;

    while (curr) {
        if (curr->state == 0) {
            // Task is dead/completed -> unlink
            if (prev) {
                prev->next = curr->next;
            } else {
                listhead = curr->next;
            }
            if (curr == listtail) {
                listtail = prev;
            }
            curr = curr->next;
            continue;
        }

        // Active task: reset stale scheduler data and hand over!
        curr->sched_data = nullptr;
        new_sched->add_task(curr);
        count++;

        prev = curr;
        curr = curr->next;
    }

    // 3. Swap the active pointer
    gk.sched = new_sched;

    // 4. Resume interrupts
    asm volatile("sti");

    kprintf("[SWAP] Switched active Scheduler to: %s (%d tasks migrated)\n", mod->name, count);
    return true;
}

bool swap_heap(struct ModuleHeader* mod) {
    if (!mod || !mod->module_init) return false;

    struct HEAP_API* new_heap = (struct HEAP_API*)mod->module_init(&kapi);
    if (!new_heap) {
        kprintf("[SWAP] Error: Failed to init heap '%s'\n", mod->name);
        return false;
    }

    gk.oldheap = gk.heap;
    gk.oldcnt = gk.heapcnt;
    gk.flag |= 1;
    gk.heap = new_heap;
    gk.heapcnt = 0;

    kprintf("[SWAP] Switched active Heap to: %s (old heap preserved for %d live allocations)\n",
            mod->name, gk.oldcnt);
    return true;
}

bool swap_pmm(struct ModuleHeader* mod) {
    if (!mod || !mod->module_init) return false;

    struct PMM_API* new_pmm = (struct PMM_API*)mod->module_init(&kapi);
    if (!new_pmm) {
        kprintf("[SWAP] Error: Failed to init PMM '%s'\n", mod->name);
        return false;
    }

    gk.pmm = new_pmm;
    kprintf("[SWAP] Switched active PMM to: %s\n", mod->name);
    return true;
}

bool swap_fs(struct ModuleHeader* mod) {
    if (!mod || !mod->module_init) return false;

    struct FS_API* new_fs = (struct FS_API*)mod->module_init(&kapi);
    if (!new_fs) {
        kprintf("[SWAP] Error: Failed to init filesystem '%s'\n", mod->name);
        return false;
    }

    gk.fs = new_fs;
    if (gk.current_disk) {
        gk.fs->mount(gk.current_disk);
    }

    kprintf("[SWAP] Switched active Filesystem to: %s\n", mod->name);
    return true;
}

bool swap_blockdev(struct ModuleHeader* mod) {
    if (!mod || !mod->module_init) return false;

    struct BLOCKDEV_API* new_dev = (struct BLOCKDEV_API*)mod->module_init(&kapi);
    if (!new_dev) {
        kprintf("[SWAP] Error: Failed to init block device '%s'\n", mod->name);
        return false;
    }

    bool exists = false;
    for (uint32_t i = 0; i < gk.disk_count; i++) {
        if (gk.disks[i] == new_dev) {
            exists = true;
            break;
        }
    }
    if (!exists && gk.disk_count < 8) {
        gk.disks[gk.disk_count++] = new_dev;
    }

    gk.current_disk = new_dev;
    if (gk.fs) {
        gk.fs->mount(new_dev);
    }

    kprintf("[SWAP] Switched active Drive to: %s (%s)\n", mod->name, new_dev->name);
    return true;
}

bool swap_module(struct ModuleHeader* mod) {
    if (!mod) return false;

    switch (mod->type) {
        case MT_SCHEDULER: return swap_scheduler(mod);
        case MT_HEAP:      return swap_heap(mod);
        case MT_PMM:       return swap_pmm(mod);
        case MT_FS:        return swap_fs(mod);
        case MT_BLOCKDEV:  return swap_blockdev(mod);
        default:
            kprintf("[SWAP] Error: Unknown module type: %d\n", mod->type);
            return false;
    }
}
