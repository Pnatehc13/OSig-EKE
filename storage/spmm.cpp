#include "spmm.h"
#include "../kernel/kernel_api.h"
#include "../kernel/tty.h"

extern struct Kernel gk;

static inline void setbit(uint64_t* bm, uint64_t block) {
    bm[block / 64] |= (1ULL << (block % 64));
}

static inline void clearbit(uint64_t* bm, uint64_t block) {
    bm[block / 64] &= ~(1ULL << (block % 64));
}

static inline int getbit(const uint64_t* bm, uint64_t block) {
    return (bm[block / 64] >> (block % 64)) & 1ULL;
}

static bool str_eq(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++; b++;
    }
    return *a == *b;
}

void spmm_init(struct BLOCKDEV_API* dev, uint64_t* bitmap_buf, uint64_t total_blocks, bool all_free) {
    if (!dev || !bitmap_buf) return;

    dev->bitmap = bitmap_buf;
    dev->total_blocks = total_blocks;
    dev->last_free_block = 1; // Block 0 is always reserved for Superblock

    uint64_t total_words = (total_blocks + 63) / 64;

    if (all_free) {
        for (uint64_t i = 0; i < total_words; i++) {
            dev->bitmap[i] = 0;
        }
        dev->free_blocks = total_blocks;

        // Reserve Block 0 for Superblock
        setbit(dev->bitmap, 0);
        dev->free_blocks--;
    } else {
        // Count free blocks from loaded bitmap
        uint64_t free_cnt = 0;
        for (uint64_t b = 0; b < total_blocks; b++) {
            if (!getbit(dev->bitmap, b)) {
                free_cnt++;
            }
        }
        dev->free_blocks = free_cnt;
    }
}

uint64_t spmm_alloc(struct BLOCKDEV_API* dev) {
    if (!dev) dev = gk.current_disk;
    if (!dev || !dev->bitmap || dev->free_blocks == 0) return 0;

    asm volatile("cli");

    uint64_t total_words = (dev->total_blocks + 63) / 64;
    uint64_t start_word = dev->last_free_block / 64;

    // Scan from hint to end
    for (uint64_t w = start_word; w < total_words; w++) {
        if (dev->bitmap[w] == 0xFFFFFFFFFFFFFFFFULL) continue; // Skip 64 full blocks

        for (int bit = 0; bit < 64; bit++) {
            uint64_t b = w * 64 + bit;
            if (b == 0) continue; // Superblock
            if (b >= dev->total_blocks) break;

            if (!getbit(dev->bitmap, b)) {
                setbit(dev->bitmap, b);
                dev->free_blocks--;
                dev->last_free_block = b + 1;
                asm volatile("sti");
                return b;
            }
        }
    }

    // Wrap around from word 0 if not found
    for (uint64_t w = 0; w < start_word; w++) {
        if (dev->bitmap[w] == 0xFFFFFFFFFFFFFFFFULL) continue;

        for (int bit = 0; bit < 64; bit++) {
            uint64_t b = w * 64 + bit;
            if (b == 0) continue;
            if (b >= dev->total_blocks) break;

            if (!getbit(dev->bitmap, b)) {
                setbit(dev->bitmap, b);
                dev->free_blocks--;
                dev->last_free_block = b + 1;
                asm volatile("sti");
                return b;
            }
        }
    }

    asm volatile("sti");
    return 0; // Disk full
}

uint64_t spmm_alloc_contiguous(uint32_t count, struct BLOCKDEV_API* dev) {
    if (!dev) dev = gk.current_disk;
    if (!dev || !dev->bitmap || dev->free_blocks < count || count == 0) return 0;

    asm volatile("cli");

    uint64_t run_start = 1;
    uint32_t current_run = 0;

    for (uint64_t b = 1; b < dev->total_blocks; b++) {
        if (!getbit(dev->bitmap, b)) {
            if (current_run == 0) run_start = b;
            current_run++;
            if (current_run == count) {
                // Found enough contiguous blocks, mark them all
                for (uint64_t i = run_start; i < run_start + count; i++) {
                    setbit(dev->bitmap, i);
                }
                dev->free_blocks -= count;
                dev->last_free_block = run_start + count;
                asm volatile("sti");
                return run_start;
            }
        } else {
            current_run = 0;
        }
    }

    asm volatile("sti");
    return 0; // No contiguous chunk found
}

void spmm_free(uint64_t block, struct BLOCKDEV_API* dev) {
    if (!dev) dev = gk.current_disk;
    if (!dev || !dev->bitmap || block == 0 || block >= dev->total_blocks) return;

    asm volatile("cli");

    if (getbit(dev->bitmap, block)) {
        clearbit(dev->bitmap, block);
        dev->free_blocks++;
        if (block < dev->last_free_block) {
            dev->last_free_block = block;
        }
    }

    asm volatile("sti");
}

void spmm_free_contiguous(uint64_t start_block, uint32_t count, struct BLOCKDEV_API* dev) {
    if (!dev) dev = gk.current_disk;
    if (!dev || !dev->bitmap || start_block == 0) return;

    for (uint32_t i = 0; i < count; i++) {
        spmm_free(start_block + i, dev);
    }
}

bool spmm_is_free(uint64_t block, const struct BLOCKDEV_API* dev) {
    if (!dev) dev = gk.current_disk;
    if (!dev || !dev->bitmap || block >= dev->total_blocks) return false;
    return getbit(dev->bitmap, block) == 0;
}

void spmm_mark_used(uint64_t block, struct BLOCKDEV_API* dev) {
    if (!dev) dev = gk.current_disk;
    if (!dev || !dev->bitmap || block >= dev->total_blocks) return;

    asm volatile("cli");
    if (!getbit(dev->bitmap, block)) {
        setbit(dev->bitmap, block);
        if (dev->free_blocks > 0) dev->free_blocks--;
    }
    asm volatile("sti");
}

bool changedrive(const char* name) {
    if (!name) return false;

    for (uint32_t i = 0; i < gk.disk_count; i++) {
        if (gk.disks[i] && str_eq(gk.disks[i]->name, name)) {
            gk.current_disk = gk.disks[i];
            kprintf("[DRIVE] Active drive switched to: %s\n", name);
            return true;
        }
    }

    kprintf("[DRIVE] Error: Drive '%s' not found!\n", name);
    return false;
}
