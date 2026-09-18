#include "../../kernel/module_api.h"
#include "../../kernel/kernel_api.h"
#include "../../storage/spmm.h"
#include "../../kernel/tty.h"

#define EKE_FS_MAGIC 0x454B4546 // "EKEF"
#define SECTOR_SIZE  512
#define MAX_FILES    64

extern struct Kernel gk;

struct Superblock {
    uint32_t magic;
    uint32_t block_size;
    uint64_t total_blocks;
    uint64_t bitmap_start_lba;
    uint64_t bitmap_block_count;
    uint64_t file_table_start_lba;
    uint64_t file_table_block_count;
    uint64_t data_start_lba;
};

struct FileEntry {
    char     name[32];
    uint32_t size;
    uint32_t start_block;
    uint32_t block_count;
    uint8_t  used;
    uint8_t  reserved[23];
};

static const KernelAPI* g_api = nullptr;
static BLOCKDEV_API* active_dev = nullptr;
static Superblock active_sb;
static FileEntry* file_table = nullptr;
static uint64_t* bitmap_buf = nullptr;

// Internal helpers
static bool str_eq(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++; b++;
    }
    return *a == *b;
}

static void str_copy(char* dest, const char* src, int max_len) {
    int i = 0;
    while (src[i] && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static void fs_memcpy(void* dest, const void* src, uint64_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (uint64_t i = 0; i < n; i++) d[i] = s[i];
}

static void fs_memset(void* dest, uint8_t val, uint64_t n) {
    uint8_t* d = (uint8_t*)dest;
    for (uint64_t i = 0; i < n; i++) d[i] = val;
}

// 1. FORMAT
static int eke_fs_format(BLOCKDEV_API* dev) {
    if (!dev) return -1;

    Superblock sb;
    fs_memset(&sb, 0, sizeof(Superblock));
    sb.magic = EKE_FS_MAGIC;
    sb.block_size = SECTOR_SIZE;
    sb.total_blocks = dev->total_sectors;
    sb.bitmap_start_lba = 1;
    sb.bitmap_block_count = (sb.total_blocks / 8 + SECTOR_SIZE - 1) / SECTOR_SIZE;
    sb.file_table_start_lba = 1 + sb.bitmap_block_count;
    sb.file_table_block_count = 8; // 8 sectors = 64 file entries
    sb.data_start_lba = sb.file_table_start_lba + sb.file_table_block_count;

    // Write Superblock to Block 0
    uint8_t sector_buf[SECTOR_SIZE];
    fs_memset(sector_buf, 0, SECTOR_SIZE);
    fs_memcpy(sector_buf, &sb, sizeof(Superblock));
    dev->write_sectors(0, 1, sector_buf);

    // Clear bitmap sectors on disk
    fs_memset(sector_buf, 0, SECTOR_SIZE);
    for (uint64_t i = 0; i < sb.bitmap_block_count; i++) {
        dev->write_sectors(sb.bitmap_start_lba + i, 1, sector_buf);
    }

    // Clear file table sectors on disk
    for (uint64_t i = 0; i < sb.file_table_block_count; i++) {
        dev->write_sectors(sb.file_table_start_lba + i, 1, sector_buf);
    }

    return 0;
}

// 2. MOUNT
static int eke_fs_mount(BLOCKDEV_API* dev) {
    if (!dev) return -1;

    uint8_t sector_buf[SECTOR_SIZE];
    if (dev->read_sectors(0, 1, sector_buf) != 0) {
        kprintf("[FS] Error: Failed to read Block 0 from %s\n", dev->name);
        return -1;
    }

    Superblock* sb_ptr = (Superblock*)sector_buf;
    if (sb_ptr->magic != EKE_FS_MAGIC) {
        kprintf("[FS] Drive %s not formatted. Formatting EKE-FS...\n", dev->name);
        if (eke_fs_format(dev) != 0) {
            kprintf("[FS] Error: Format failed on %s\n", dev->name);
            return -1;
        }
        dev->read_sectors(0, 1, sector_buf);
    }

    active_sb = *sb_ptr;
    active_dev = dev;

    // Load Bitmap into RAM
    uint64_t bm_bytes = active_sb.bitmap_block_count * SECTOR_SIZE;
    uint64_t bm_pages = (bm_bytes + 4095) / 4096;
    if (!bitmap_buf && g_api) {
        bitmap_buf = (uint64_t*)g_api->alloc_page(bm_pages);
    }
    dev->read_sectors(active_sb.bitmap_start_lba, active_sb.bitmap_block_count, bitmap_buf);

    // Hook up SPMM!
    spmm_init(dev, bitmap_buf, active_sb.total_blocks, false);

    // Reserve metadata blocks (0 to data_start_lba - 1)
    for (uint64_t b = 0; b < active_sb.data_start_lba; b++) {
        spmm_mark_used(b, dev);
    }

    // Load File Table into RAM
    uint64_t ft_bytes = active_sb.file_table_block_count * SECTOR_SIZE;
    uint64_t ft_pages = (ft_bytes + 4095) / 4096;
    if (!file_table && g_api) {
        file_table = (FileEntry*)g_api->alloc_page(ft_pages);
    }
    dev->read_sectors(active_sb.file_table_start_lba, active_sb.file_table_block_count, file_table);

    kprintf("[FS] Mounted %s: %d total blocks, %d free\n",
            dev->name, (uint32_t)active_sb.total_blocks, (uint32_t)dev->free_blocks);
    return 0;
}

// 3. READ FILE
static int eke_fs_read_file(const char* path, void* buffer, uint64_t max_bytes) {
    if (!active_dev || !file_table || !path || !buffer) return -1;

    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used && str_eq(file_table[i].name, path)) {
            uint64_t to_read = file_table[i].size;
            if (to_read > max_bytes) to_read = max_bytes;
            if (to_read == 0) return 0;

            uint32_t blocks = file_table[i].block_count;
            uint64_t pages = (blocks * SECTOR_SIZE + 4095) / 4096;
            uint8_t* temp = nullptr;
            if (g_api) temp = (uint8_t*)g_api->alloc_page(pages);
            if (!temp) return -1;

            active_dev->read_sectors(file_table[i].start_block, blocks, temp);
            fs_memcpy(buffer, temp, to_read);
            if (g_api) g_api->free_page((uintptr_t)temp);

            return (int)to_read;
        }
    }
    return -1; // File not found
}

// 4. WRITE FILE
static int eke_fs_write_file(const char* path, const void* buffer, uint64_t bytes) {
    if (!active_dev || !file_table || !path || !buffer) return -1;

    uint32_t blocks = (bytes + SECTOR_SIZE - 1) / SECTOR_SIZE;
    if (blocks == 0) blocks = 1;

    FileEntry* target = nullptr;

    // Check if file exists
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used && str_eq(file_table[i].name, path)) {
            target = &file_table[i];
            spmm_free_contiguous(target->start_block, target->block_count, active_dev);
            break;
        }
    }

    // If new file, find free slot
    if (!target) {
        for (int i = 0; i < MAX_FILES; i++) {
            if (!file_table[i].used) {
                target = &file_table[i];
                break;
            }
        }
    }

    if (!target) {
        kprintf("[FS] Error: File table full!\n");
        return -2;
    }

    // Allocate blocks from SPMM
    uint64_t start_block = spmm_alloc_contiguous(blocks, active_dev);
    if (start_block == 0) {
        kprintf("[FS] Error: Disk full!\n");
        return -3;
    }

    // Prepare buffer
    uint64_t pages = (blocks * SECTOR_SIZE + 4095) / 4096;
    uint8_t* temp = nullptr;
    if (g_api) temp = (uint8_t*)g_api->alloc_page(pages);
    if (!temp) return -1;

    fs_memset(temp, 0, blocks * SECTOR_SIZE);
    fs_memcpy(temp, buffer, bytes);

    // Write to disk
    active_dev->write_sectors(start_block, blocks, temp);
    if (g_api) g_api->free_page((uintptr_t)temp);

    // Update File Table
    str_copy(target->name, path, 32);
    target->size = bytes;
    target->start_block = start_block;
    target->block_count = blocks;
    target->used = 1;

    // Flush metadata to disk
    active_dev->write_sectors(active_sb.bitmap_start_lba, active_sb.bitmap_block_count, active_dev->bitmap);
    active_dev->write_sectors(active_sb.file_table_start_lba, active_sb.file_table_block_count, file_table);

    return 0;
}

// 5. GET SIZE
static uint64_t eke_fs_get_size(const char* path) {
    if (!file_table || !path) return 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used && str_eq(file_table[i].name, path)) {
            return file_table[i].size;
        }
    }
    return 0;
}

// 6. DELETE FILE
static int eke_fs_delete_file(const char* path) {
    if (!active_dev || !file_table || !path) return -1;

    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used && str_eq(file_table[i].name, path)) {
            spmm_free_contiguous(file_table[i].start_block, file_table[i].block_count, active_dev);
            file_table[i].used = 0;
            file_table[i].name[0] = '\0';

            // Flush metadata
            active_dev->write_sectors(active_sb.bitmap_start_lba, active_sb.bitmap_block_count, active_dev->bitmap);
            active_dev->write_sectors(active_sb.file_table_start_lba, active_sb.file_table_block_count, file_table);
            return 0;
        }
    }
    return -1;
}

// 7. LIST FILES
static int eke_fs_list_files(void (*callback)(const char* filename, uint64_t size)) {
    if (!file_table || !callback) return 0;
    int count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used) {
            callback(file_table[i].name, file_table[i].size);
            count++;
        }
    }
    return count;
}

static struct FS_API eke_fs_api = {
    .name = "eke_fs",
    .mount = eke_fs_mount,
    .read_file = eke_fs_read_file,
    .write_file = eke_fs_write_file,
    .get_size = eke_fs_get_size,
    .delete_file = eke_fs_delete_file,
    .list_files = eke_fs_list_files
};

void* eke_fs_init(const KernelAPI* api) {
    g_api = api;

    // Auto-mount active drive if already present
    if (gk.current_disk) {
        eke_fs_mount(gk.current_disk);
    }

    return (void*)&eke_fs_api;
}

__attribute__((section(".modules"))) ModuleHeader eke_fs_mod = {
    MAGICNUM,
    "EKE-FS",
    MT_FS,
    eke_fs_init
};
