#include "../../kernel/module_api.h"
#include "../../kernel/kernel_api.h"

// Hardware I/O Port Instructions
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

// Polling helpers
static int ata_wait_not_busy() {
    for (int timeout = 0; timeout < 100000; timeout++) {
        uint8_t status = inb(0x1F7);
        if (!(status & 0x80)) return 0; // BSY bit cleared
    }
    return -1; // Timeout
}

static int ata_wait_ready() {
    for (int timeout = 0; timeout < 100000; timeout++) {
        uint8_t status = inb(0x1F7);
        if (status & 0x01) return -1; // ERR bit set
        if (!(status & 0x80) && (status & 0x08)) return 0; // BSY=0 and DRQ=1 (Data Ready!)
    }
    return -1; // Timeout
}

// 1. READ ONE SECTOR (512 bytes)
static int ata_read_one_sector(uint32_t lba, void* buf) {
    if (ata_wait_not_busy() != 0) return -1;

    // Select Drive 0 (Master) and highest 4 bits of LBA
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);                            // Sector count = 1
    outb(0x1F3, (uint8_t)(lba & 0xFF));        // LBA bits 0-7
    outb(0x1F4, (uint8_t)((lba >> 8) & 0xFF)); // LBA bits 8-15
    outb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));// LBA bits 16-23
    outb(0x1F7, 0x20);                         // ATA Command 0x20: READ SECTORS

    if (ata_wait_ready() != 0) return -1;

    // Read 256 16-bit words (512 bytes) from Data Port 0x1F0
    uint16_t* ptr = (uint16_t*)buf;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(0x1F0);
    }
    return 0;
}

// 2. WRITE ONE SECTOR (512 bytes)
static int ata_write_one_sector(uint32_t lba, const void* buf) {
    if (ata_wait_not_busy() != 0) return -1;

    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)(lba & 0xFF));
    outb(0x1F4, (uint8_t)((lba >> 8) & 0xFF));
    outb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));
    outb(0x1F7, 0x30); // ATA Command 0x30: WRITE SECTORS

    if (ata_wait_ready() != 0) return -1;

    // Write 256 16-bit words (512 bytes) to Data Port 0x1F0
    const uint16_t* ptr = (const uint16_t*)buf;
    for (int i = 0; i < 256; i++) {
        outw(0x1F0, ptr[i]);
    }

    // Flush drive cache so changes hit physical media
    outb(0x1F7, 0xE7);
    ata_wait_not_busy();

    return 0;
}

// BLOCKDEV_API Multi-sector wrappers
static int ata_read_sectors(uint64_t lba, uint32_t count, void* buffer) {
    uint8_t* ptr = (uint8_t*)buffer;
    for (uint32_t i = 0; i < count; i++) {
        if (ata_read_one_sector((uint32_t)(lba + i), ptr + (i * 512)) != 0) {
            return -1;
        }
    }
    return 0;
}

static int ata_write_sectors(uint64_t lba, uint32_t count, const void* buffer) {
    const uint8_t* ptr = (const uint8_t*)buffer;
    for (uint32_t i = 0; i < count; i++) {
        if (ata_write_one_sector((uint32_t)(lba + i), ptr + (i * 512)) != 0) {
            return -1;
        }
    }
    return 0;
}

// Detect drive presence and capacity via ATA IDENTIFY (0xEC)
static uint64_t ata_probe() {
    outb(0x1F6, 0xA0); // Master drive
    outb(0x1F2, 0);
    outb(0x1F3, 0);
    outb(0x1F4, 0);
    outb(0x1F5, 0);
    outb(0x1F7, 0xEC); // IDENTIFY command

    uint8_t status = inb(0x1F7);
    if (status == 0 || status == 0xFF) {
        return 0; // No drive connected
    }

    if (ata_wait_ready() != 0) {
        return 0;
    }

    uint16_t id_buf[256];
    for (int i = 0; i < 256; i++) {
        id_buf[i] = inw(0x1F0);
    }

    // Words 60-61 hold total 28-bit sector count
    uint32_t sectors = (uint32_t)id_buf[60] | ((uint32_t)id_buf[61] << 16);
    if (sectors > 0) return sectors;

    return 131072; // Default 64 MB (131,072 sectors * 512 bytes)
}

static BLOCKDEV_API ata_dev = {
    .name = "ata0",
    .total_sectors = 131072,
    .sector_size = 512,
    .read_sectors = ata_read_sectors,
    .write_sectors = ata_write_sectors,
    .bitmap = nullptr,
    .total_blocks = 0,
    .free_blocks = 0,
    .last_free_block = 0
};

void* ata_init(const KernelAPI* api) {
    uint64_t sectors = ata_probe();
    if (sectors == 0) {
        // No hardware drive attached
        return nullptr;
    }

    ata_dev.total_sectors = sectors;
    return (void*)&ata_dev;
}

__attribute__((section(".modules"))) ModuleHeader ata_mod = {
    MAGICNUM,
    "ATADriver",
    MT_BLOCKDEV,
    ata_init
};
