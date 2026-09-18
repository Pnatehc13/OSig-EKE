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
	MT_FS = 5,
	MT_BLOCKDEV = 6
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


struct BLOCKDEV_API 
{
    const char* name;             // e.g. "ram0", "ata0"
    uint64_t total_sectors;       // 64-bit sector count (supports >2TB drives)
    uint32_t sector_size;         // standard 512 bytes
    
    // Sector-level I/O (0 on success, negative on error)
    int (*read_sectors)(uint64_t lba, uint32_t count, void* buffer);
    int (*write_sectors)(uint64_t lba, uint32_t count, const void* buffer);

    // In-memory allocation state (its SPMM, populated on mount)
    uint64_t* bitmap;             // Pointer to in-memory block bitmap
    uint64_t  total_blocks;       // Total blocks managed by FS
    uint64_t  free_blocks;        // Count of free blocks
    uint64_t  last_free_block;    // Search hint for allocation
};

struct FS_API 
{
    const char* name;             // e.g. "eke_fs"
    
    // Mount a drive
    int (*mount)(struct BLOCKDEV_API* dev);
    
    // Whole-file operations
    int (*read_file)(const char* path, void* buffer, uint64_t max_bytes);
    int (*write_file)(const char* path, const void* buffer, uint64_t bytes);
    uint64_t (*get_size)(const char* path);
    int (*delete_file)(const char* path);
    
    // List directory contents: calls callback for each file found
    int (*list_files)(void (*callback)(const char* filename, uint64_t size));
};

typedef void* (*module_init_t)(const KernelAPI* api);
#endif
