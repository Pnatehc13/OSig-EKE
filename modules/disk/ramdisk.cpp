#include "../../kernel/module_api.h"
#include "../../kernel/kernel_api.h"

#define RAMDISK_PAGES 4096                  // 4096 pages * 4KB = 16 MB of disk
#define SECTOR_SIZE   512
#define TOTAL_SECTORS (RAMDISK_PAGES * 4096 / SECTOR_SIZE) // 32,768 sectors

static uint8_t* ramdisk_buf = nullptr;

// Simple freestanding memcpy
static void ram_memcpy(void* dest, const void* src, uint64_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (uint64_t i = 0; i < n; i++) d[i] = s[i];
}

// 1. READ SECTORS: Just copy from RAM buffer into user's buffer!
static int ramdisk_read_sectors(uint64_t lba, uint32_t count, void* buffer) {
    if (lba + count > TOTAL_SECTORS) return -1; // Out of bounds
    
    uint64_t offset = lba * SECTOR_SIZE;
    ram_memcpy(buffer, ramdisk_buf + offset, count * SECTOR_SIZE);
    return 0;
}

// 2. WRITE SECTORS: Just copy from user's buffer into RAM buffer!
static int ramdisk_write_sectors(uint64_t lba, uint32_t count, const void* buffer) {
    if (lba + count > TOTAL_SECTORS) return -1; // Out of bounds
    
    uint64_t offset = lba * SECTOR_SIZE;
    ram_memcpy(ramdisk_buf + offset, buffer, count * SECTOR_SIZE);
    return 0;
}

// 3. THE BLOCKDEV_API INSTANCE
static BLOCKDEV_API ramdisk_dev = {
    .name = "ram0",
    .total_sectors = TOTAL_SECTORS,
    .sector_size = SECTOR_SIZE,
    .read_sectors = ramdisk_read_sectors,
    .write_sectors = ramdisk_write_sectors,
    .bitmap = nullptr,
    .total_blocks = 0,
    .free_blocks = 0,
    .last_free_block = 0
};

// 4. MODULE INIT & HEADER
void* ramdisk_init(const KernelAPI* api) {
    // Allocate 16 MB of physical RAM from our kernel PMM
    ramdisk_buf = (uint8_t*)api->alloc_page(RAMDISK_PAGES);
    if (!ramdisk_buf) return nullptr;

    // Zero out the RAM disk
    for (uint64_t i = 0; i < RAMDISK_PAGES * 4096; i++) {
        ramdisk_buf[i] = 0;
    }

    return (void*)&ramdisk_dev;
}

__attribute__((section(".modules"))) ModuleHeader ramdisk_mod = {
    MAGICNUM,
    "RAMDiskDriver",
    MT_BLOCKDEV,
    ramdisk_init
};
