#ifndef SPMM_H
#define SPMM_H

#include <stdint.h>
#include <stdbool.h>
#include "../kernel/module_api.h"

// Initialize a drive's SPMM with an in-memory bitmap buffer
void spmm_init(struct BLOCKDEV_API* dev, uint64_t* bitmap_buf, uint64_t total_blocks, bool all_free);

// Allocate 1 block (defaults to gk.current_disk if dev is null)
// Returns 0 on failure/disk full (since Block 0 is the Superblock)
uint64_t spmm_alloc(struct BLOCKDEV_API* dev = nullptr);

// Allocate N contiguous blocks
uint64_t spmm_alloc_contiguous(uint32_t count, struct BLOCKDEV_API* dev = nullptr);

// Free 1 block
void spmm_free(uint64_t block, struct BLOCKDEV_API* dev = nullptr);

// Free N contiguous blocks
void spmm_free_contiguous(uint64_t start_block, uint32_t count, struct BLOCKDEV_API* dev = nullptr);

// Check if a block is free (true = free, false = used)
bool spmm_is_free(uint64_t block, const struct BLOCKDEV_API* dev = nullptr);

// Manually mark a block as used (to reserve superblock, bitmap blocks, etc.)
void spmm_mark_used(uint64_t block, struct BLOCKDEV_API* dev = nullptr);

// Switch the active drive channel by name (e.g. "ram0", "ata0", "usb")
bool changedrive(const char* name);

#endif
