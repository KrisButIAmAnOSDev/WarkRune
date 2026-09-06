// kernel/fs/fat12.cpp  --  Simple read-write FAT12 filesystem driver
//
// FAT12 is the simplest FAT variant:
//   * 12-bit cluster entries packed in pairs (3 bytes per 2 entries)
//   * 8.3 short filenames only (uppercase, space-padded)
//   * Fixed-size 32-byte directory entries
//   * Two FAT copies for redundancy
//   * Root directory is a fixed region (not a cluster chain)
//
// Design constraints (baremetal, no libc, single-threaded):
//   * One shared 512-byte sector buffer (g_buf).
//   * Callers must not hold pointers into g_buf across disk reads.
//   * Single-threaded: no locking needed.

#include "fs/fat12.hpp"
#include "io/ata.hpp"

// ============================================================
// On-disk structures
// ============================================================

struct FAT12BPB {
    uint8_t  jump[3];          //   0
    char     oem[8];           //   3
    uint16_t bytes_per_sec;    //  11
    uint8_t  sec_per_clus;     //  13
    uint16_t reserved_sec;     //  14
    uint8_t  num_fats;         //  16
    uint16_t root_entries;     //  17
    uint16_t total_sec16;      //  19
    uint8_t  media_type;       //  21
    uint16_t fat_size16;       //  22
    uint16_t sec_per_track;    //  24
    uint16_t num_heads;        //  26
    uint32_t hidden_sec;       //  28
    uint32_t total_sec32;      //  32
    uint8_t  drive_num;        //  36
    uint8_t  reserved1;        //  37
    uint8_t  boot_sig;         //  38
    uint32_t volume_id;        //  39
    char     volume_label[11]; //  43
    char     fs_type[8];       //  54
} __attribute__((packed));

struct DirEntry {
    char     name[8];          //  0-7   filename, space-padded
    char     ext[3];           //  8-10  extension, space-padded
    uint8_t  attr;             //  11    attributes
    uint8_t  nt_reserved;      //  12
    uint8_t  create_time_tenth;//  13
    uint16_t create_time;      //  14
    uint16_t create_date;      //  16
    uint16_t access_date;      //  18
    uint16_t first_cluster_hi; //  20    always 0 for FAT12
    uint16_t write_time;       //  22
    uint16_t write_date;       //  24
    uint16_t first_cluster;    //  26
    uint32_t file_size;        //  28
} __attribute__((packed));

// Directory entry attributes
#define ATTR_DIR   0x10
#define ATTR_ARCH  0x20
#define ATTR_LFN   0x0F

// FAT12 special cluster values
#define FAT12_FREE     0x000
#define FAT12_RESERVED 0x001
#define FAT12_EOC      0xFF8   // 0xFF8..0xFFF = end of chain
#define FAT12_BAD      0xFF7

// ============================================================
// Global driver state
// ============================================================

static uint32_t g_part_lba;       // LBA of partition start (VBR)
static uint32_t g_fat_lba;        // LBA of FAT[0] (first copy)
static uint32_t g_root_lba;       // LBA of root directory
static uint32_t g_data_lba;       // LBA of first data cluster
static uint16_t g_root_entries;   // max root dir entries
static uint16_t g_sec_per_clus;   // sectors per cluster
static uint16_t g_reserved_sec;   // reserved sectors
static uint8_t  g_num_fats;       // number of FAT copies
static uint16_t g_fat_size;       // FAT size in sectors
static uint32_t g_total_clusters; // total data clusters

static uint8_t g_buf[512];       // shared sector buffer

// ============================================================
// File descriptor table
// ============================================================

struct FD {
    bool     used;
    uint16_t first_cluster;
    uint32_t size;
    uint32_t pos;
    uint16_t parent_cluster;  // cluster of parent directory (0 = root)
    int      dir_idx;         // absolute entry index in parent directory
};

static FD g_fds[FAT12_MAX_FDS];

// ============================================================
// Low-level disk I/O
// ============================================================

static bool read_sector(uint32_t lba) {
    return ata_read(lba, 1, (uint16_t*)g_buf);
}

static bool write_sector(uint32_t lba) {
    return ata_write(lba, 1, (uint16_t*)g_buf);
}

// ============================================================
// Cluster geometry helpers
// ============================================================

static uint32_t cluster_lba(uint16_t cluster) {
    return g_data_lba + (uint32_t)(cluster - 2) * g_sec_per_clus;
}

static uint32_t cluster_size() {
    return (uint32_t)g_sec_per_clus * 512;
}

// ============================================================
// FAT12 cluster chain helpers
// ============================================================

// Read a 12-bit FAT entry.  Reads the sector(s) containing the entry.
static uint16_t fat_get(uint16_t cluster) {
    // FAT12 stores 2 entries per 3 bytes
    uint32_t byte_off = (uint32_t)cluster * 3 / 2;
    uint32_t sec_off  = byte_off / 512;
    uint32_t in_sec   = byte_off % 512;

    read_sector(g_fat_lba + sec_off);

    uint16_t entry;
    if (in_sec == 511) {
        // Entry straddles two sectors
        uint8_t lo = g_buf[511];
        read_sector(g_fat_lba + sec_off + 1);
        uint8_t hi = g_buf[0];
        if (cluster & 1)
            entry = ((uint16_t)lo >> 4) | ((uint16_t)hi << 4);
        else
            entry = lo | ((uint16_t)(hi & 0x0F) << 8);
    } else {
        uint8_t b0 = g_buf[in_sec];
        uint8_t b1 = g_buf[in_sec + 1];
        if (cluster & 1)
            entry = ((uint16_t)b0 >> 4) | ((uint16_t)b1 << 4);
        else
            entry = b0 | ((uint16_t)(b1 & 0x0F) << 8);
    }
    return entry;
}

// Write a 12-bit FAT entry.  Reads-modify-writes the sector(s).
static void fat_set(uint16_t cluster, uint16_t val) {
    uint32_t byte_off = (uint32_t)cluster * 3 / 2;
    uint32_t sec_off  = byte_off / 512;
    uint32_t in_sec   = byte_off % 512;

    // Helper: patch one FAT copy
    auto write_fat = [&](uint32_t fat_base) {
        if (in_sec == 511) {
            // Entry straddles two sectors
            read_sector(fat_base + sec_off);
            uint8_t lo = g_buf[511];
            read_sector(fat_base + sec_off + 1);
            uint8_t hi = g_buf[0];

            if (cluster & 1) {
                lo = (lo & 0x0F) | ((uint8_t)(val << 4));
                hi = (uint8_t)(val >> 4);
            } else {
                lo = (uint8_t)(val & 0xFF);
                hi = (hi & 0xF0) | (uint8_t)((val >> 8) & 0x0F);
            }

            read_sector(fat_base + sec_off);
            g_buf[511] = lo;
            write_sector(fat_base + sec_off);

            read_sector(fat_base + sec_off + 1);
            g_buf[0] = hi;
            write_sector(fat_base + sec_off + 1);
        } else {
            read_sector(fat_base + sec_off);
            if (cluster & 1) {
                g_buf[in_sec]     = (g_buf[in_sec] & 0x0F) | ((uint8_t)(val << 4));
                g_buf[in_sec + 1] = (uint8_t)(val >> 4);
            } else {
                g_buf[in_sec]     = (uint8_t)(val & 0xFF);
                g_buf[in_sec + 1] = (g_buf[in_sec + 1] & 0xF0) | (uint8_t)((val >> 8) & 0x0F);
            }
            write_sector(fat_base + sec_off);
        }
    };

    write_fat(g_fat_lba);
    if (g_num_fats > 1)
        write_fat(g_fat_lba + g_fat_size);
}

// Scan FAT for a free cluster, mark it EOC, return its number (0 = full).
static uint16_t fat_alloc() {
    for (uint16_t i = 2; i < g_total_clusters + 2; i++) {
        if (fat_get(i) == FAT12_FREE) {
            fat_set(i, FAT12_EOC);
            return i;
        }
    }
    return 0;
}

// Walk a cluster chain and free every entry.
static void fat_free_chain(uint16_t cluster) {
    while (cluster >= 2 && cluster < FAT12_BAD) {
        uint16_t next = fat_get(cluster);
        fat_set(cluster, FAT12_FREE);
        if (next >= FAT12_EOC) break;
        cluster = next;
    }
}

// ============================================================
// Name helpers
// ============================================================

// Convert a filename like "file.txt" to 8.3 format "FILE    TXT"
// Returns false if the name is invalid.
static bool to_83(const char* in, char name[11]) {
    for (int i = 0; i < 11; i++) name[i] = ' ';

    if (!in || in[0] == 0 || in[0] == '.') return false;

    int i = 0;
    // Copy name part (up to 8 chars, stop at dot)
    while (in[i] && in[i] != '.' && i < 8) {
        char c = in[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        name[i] = c;
        i++;
    }

    // Skip to dot
    int dot_pos = i;
    while (in[dot_pos] && in[dot_pos] != '.') dot_pos++;

    // Copy extension (up to 3 chars after dot)
    if (in[dot_pos] == '.') {
        dot_pos++;
        int j = 8;
        while (in[dot_pos] && j < 11) {
            char c = in[dot_pos];
            if (c >= 'a' && c <= 'z') c -= 32;
            name[j] = c;
            j++;
            dot_pos++;
        }
    }

    return true;
}

// Compare two 8.3 names (case-insensitive)
static bool match_83(const char* entry_name, const char* target_83) {
    for (int i = 0; i < 11; i++) {
        char a = entry_name[i], b = target_83[i];
        if (a >= 'a' && a <= 'z') a -= 32;
        if (b >= 'a' && b <= 'z') b -= 32;
        if (a != b) return false;
    }
    return true;
}

// Format a 8.3 name back to "NAME.EXT" for display
static void from_83(const char raw[11], char* out) {
    int j = 0;
    for (int i = 0; i < 8 && raw[i] != ' '; i++) out[j++] = raw[i];
    if (raw[8] != ' ') {
        out[j++] = '.';
        for (int i = 8; i < 11 && raw[i] != ' '; i++) out[j++] = raw[i];
    }
    out[j] = 0;
}

// Case-insensitive string compare
static bool streq(const char* a, const char* b) {
    for (int i = 0;; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return false;
        if (ca == 0) return true;
    }
}

static int slen(const char* s) { int i = 0; while (s[i]) i++; return i; }

// ============================================================
// Directory entry I/O
//
// Root directory lives at a fixed LBA range.
// Subdirectories use cluster chains like files.
// ============================================================

// Read a 32-byte directory entry.
// For root: idx counts from 0 across root_dir_sectors.
// For subdirs: follows cluster chain.
static bool dir_read_entry(uint32_t dir_start_lba, uint16_t dir_cluster,
                           int idx, DirEntry* out)
{
    if (dir_cluster == 0) {
        // Root directory: flat array at fixed LBA
        uint32_t sec_off = idx / 16;
        int      ent_off = idx % 16;
        if (idx >= g_root_entries) return false;
        read_sector(dir_start_lba + sec_off);
        for (int i = 0; i < 32; i++)
            ((uint8_t*)out)[i] = g_buf[ent_off * 32 + i];
        return true;
    }

    // Subdirectory: follow cluster chain
    uint32_t entries_per_clus = cluster_size() / 32;
    uint16_t clus = dir_cluster;
    uint32_t skip = idx / entries_per_clus;
    for (uint32_t i = 0; i < skip; i++) {
        clus = fat_get(clus);
        if (clus >= FAT12_EOC) return false;
    }
    int ent_in_clus = idx % entries_per_clus;
    uint32_t sec_in_clus = ent_in_clus / 16;
    int      ent_in_sec  = ent_in_clus % 16;

    read_sector(cluster_lba(clus) + sec_in_clus);
    for (int i = 0; i < 32; i++)
        ((uint8_t*)out)[i] = g_buf[ent_in_sec * 32 + i];
    return true;
}

// Write a 32-byte directory entry (same addressing as dir_read_entry).
static bool dir_write_entry(uint32_t dir_start_lba, uint16_t dir_cluster,
                            int idx, const DirEntry* in)
{
    if (dir_cluster == 0) {
        uint32_t sec_off = idx / 16;
        int      ent_off = idx % 16;
        if (idx >= g_root_entries) return false;
        read_sector(dir_start_lba + sec_off);
        for (int i = 0; i < 32; i++)
            g_buf[ent_off * 32 + i] = ((const uint8_t*)in)[i];
        write_sector(dir_start_lba + sec_off);
        return true;
    }

    uint32_t entries_per_clus = cluster_size() / 32;
    uint16_t clus = dir_cluster;
    uint32_t skip = idx / entries_per_clus;
    for (uint32_t i = 0; i < skip; i++) {
        clus = fat_get(clus);
        if (clus >= FAT12_EOC) return false;
    }
    int ent_in_clus = idx % entries_per_clus;
    uint32_t sec_in_clus = ent_in_clus / 16;
    int      ent_in_sec  = ent_in_clus % 16;

    uint32_t lba = cluster_lba(clus) + sec_in_clus;
    read_sector(lba);
    for (int i = 0; i < 32; i++)
        g_buf[ent_in_sec * 32 + i] = ((const uint8_t*)in)[i];
    write_sector(lba);
    return true;
}

// Find the total number of entries in a directory (root or subdir).
// Stops at the first free (0x00) entry.
static int dir_count_entries(uint32_t dir_start_lba, uint16_t dir_cluster) {
    DirEntry e;
    int count = 0;
    int max = (dir_cluster == 0) ? g_root_entries : 10000;
    for (int i = 0; i < max; i++) {
        if (!dir_read_entry(dir_start_lba, dir_cluster, i, &e)) break;
        if ((uint8_t)e.name[0] == 0x00) break;  // end of dir
        if ((uint8_t)e.name[0] == 0xE5) continue; // deleted
        count++;
    }
    return count;
}

// ============================================================
// Path resolution
//
// Given "/a/b/c.txt", resolves to (dir_cluster, "C     TXT").
// dir_cluster=0 means root directory.
// ============================================================

static bool resolve_path(const char* path, uint16_t* dir_cluster,
                          char out_83[11])
{
    if (path[0] == '/') path++;
    *dir_cluster = 0;  // start at root

    for (;;) {
        // Extract next component
        int len = 0;
        while (path[len] && path[len] != '/') len++;

        if (len == 0) {
            if (path[0] == 0) {
                // Empty name at end -- caller wants root
                out_83[0] = 0;
                return true;
            }
            path++;
            continue;
        }

        bool is_last = (path[len] == 0);
        char comp[256] = {};
        for (int i = 0; i < len && i < 255; i++) comp[i] = path[i];

        if (is_last) {
            to_83(comp, out_83);
            return true;
        }

        // Intermediate component: must be a directory
        char target[11];
        to_83(comp, target);

        // Search current directory for this component
        DirEntry e;
        bool found = false;
        uint32_t max_ents = (*dir_cluster == 0) ? g_root_entries : 10000;
        for (int i = 0; i < (int)max_ents; i++) {
            if (!dir_read_entry(g_root_lba, *dir_cluster, i, &e)) break;
            if ((uint8_t)e.name[0] == 0x00) break;
            if ((uint8_t)e.name[0] == 0xE5) continue;
            if (e.attr & ATTR_LFN) continue;
            if (!match_83(e.name, target)) continue;
            if (!(e.attr & ATTR_DIR)) return false;
            *dir_cluster = e.first_cluster;
            found = true;
            break;
        }
        if (!found) return false;

        path += len + 1;
    }
}

// ============================================================
// Find a file/dir in a directory.  Returns entry index or -1.
// ============================================================

static int find_in_dir(uint16_t dir_cluster, const char target_83[11],
                        DirEntry* out, int* out_idx)
{
    uint32_t max_ents = (dir_cluster == 0) ? g_root_entries : 10000;
    for (int i = 0; i < (int)max_ents; i++) {
        DirEntry e;
        if (!dir_read_entry(g_root_lba, dir_cluster, i, &e)) break;
        if ((uint8_t)e.name[0] == 0x00) break;
        if ((uint8_t)e.name[0] == 0xE5) continue;
        if (e.attr & ATTR_LFN) continue;
        if (!match_83(e.name, target_83)) continue;
        if (out) *out = e;
        if (out_idx) *out_idx = i;
        return i;
    }
    return -1;
}

// ============================================================
// Allocate a free directory entry.  Returns entry index, or -1.
// Extends subdirectory chain if needed.
// ============================================================

// Allocate a free directory entry slot.  Returns absolute entry index, or -1.
static int alloc_dir_entry(uint16_t dir_cluster) {
    uint32_t max_ents = (dir_cluster == 0) ? g_root_entries : 10000;
    for (int i = 0; i < (int)max_ents; i++) {
        DirEntry e;
        if (!dir_read_entry(g_root_lba, dir_cluster, i, &e)) break;
        uint8_t first = (uint8_t)e.name[0];
        if (first == 0x00 || first == 0xE5) return i;
    }

    // For subdirectories, extend the chain
    if (dir_cluster != 0) {
        uint16_t clus = dir_cluster;
        while (clus >= 2 && clus < FAT12_EOC) {
            uint16_t next = fat_get(clus);
            if (next >= FAT12_EOC) break;
            clus = next;
        }

        uint16_t new_clus = fat_alloc();
        if (!new_clus) return -1;
        fat_set(clus, new_clus);

        for (int i = 0; i < 512; i++) g_buf[i] = 0;
        uint32_t clba = cluster_lba(new_clus);
        for (uint32_t s = 0; s < g_sec_per_clus; s++)
            ata_write(clba + s, 1, (uint16_t*)g_buf);

        return (int)max_ents;
    }

    return -1;  // root directory is full
}

// ============================================================
// Public API
// ============================================================

bool fat12_init(uint32_t lba_start) {
    g_part_lba = lba_start;

    if (!read_sector(lba_start)) return false;

    // Try to read BPB from sector 0
    FAT12BPB* bpb = (FAT12BPB*)g_buf;
    bool has_bpb = (g_buf[510] == 0x55 && g_buf[511] == 0xAA &&
                    bpb->bytes_per_sec == 512 &&
                    bpb->sec_per_clus > 0 &&
                    bpb->reserved_sec > 0 &&
                    bpb->fat_size16 > 0);

    if (has_bpb) {
        g_sec_per_clus  = bpb->sec_per_clus;
        g_reserved_sec  = bpb->reserved_sec;
        g_num_fats      = bpb->num_fats;
        g_root_entries  = bpb->root_entries;
        g_fat_size      = bpb->fat_size16;
    } else {
        // Hardcoded geometry for 16MB FAT12 (mkfs.fat -F 12 -s 16)
        // FAT12 image placed at lba_start (256) via dd
        g_sec_per_clus  = 16;     // 8KB clusters
        g_reserved_sec  = 16;     // BPB reserved sectors (relative to FAT12 start)
        g_num_fats      = 2;
        g_root_entries  = 512;
        g_fat_size      = 16;     // 16 sectors per FAT
    }

    // Compute geometry
    g_fat_lba     = lba_start + g_reserved_sec;
    uint32_t root_dir_sectors = ((uint32_t)g_root_entries * 32 + 511) / 512;
    g_root_lba    = g_fat_lba + (uint32_t)g_num_fats * g_fat_size;
    g_data_lba    = g_root_lba + root_dir_sectors;

    uint32_t total_sectors;
    if (has_bpb)
        total_sectors = bpb->total_sec16 ? bpb->total_sec16 : bpb->total_sec32;
    else
        total_sectors = 32768;  // 16MB / 512

    if (total_sectors <= g_data_lba - lba_start) return false;

    uint32_t data_sectors = total_sectors - (g_data_lba - lba_start);
    g_total_clusters = data_sectors / g_sec_per_clus;
    if (g_total_clusters == 0) return false;

    // Verify FAT12: total clusters must be <= 4085
    if (g_total_clusters > 4085) return false;

    // Zero all FDs
    for (int i = 0; i < FAT12_MAX_FDS; i++) g_fds[i].used = false;

    return true;
}

// ============================================================
// Open
// ============================================================

int fat12_open(const char* path) {
    uint16_t dir_cluster;
    char target[11];
    if (!resolve_path(path, &dir_cluster, target)) return -1;
    if (target[0] == 0) return -1;  // can't open root as file

    DirEntry e;
    int idx = find_in_dir(dir_cluster, target, &e, nullptr);
    if (idx < 0) return -1;
    if (e.attr & ATTR_DIR) return -1;

    for (int fd = 0; fd < FAT12_MAX_FDS; fd++) {
        if (!g_fds[fd].used) {
            g_fds[fd].used          = true;
            g_fds[fd].first_cluster = e.first_cluster;
            g_fds[fd].size          = e.file_size;
            g_fds[fd].pos           = 0;
            g_fds[fd].parent_cluster = dir_cluster;
            g_fds[fd].dir_idx       = idx;
            return fd;
        }
    }
    return -1;
}

// ============================================================
// Create
// ============================================================

int fat12_create(const char* path) {
    uint16_t dir_cluster;
    char target[11];
    if (!resolve_path(path, &dir_cluster, target)) return -1;
    if (target[0] == 0) return -1;

    // Check if already exists
    if (find_in_dir(dir_cluster, target, nullptr, nullptr) >= 0) return -1;

    int idx = alloc_dir_entry(dir_cluster);
    if (idx < 0) return -1;

    DirEntry e = {};
    for (int i = 0; i < 8;  i++) e.name[i] = target[i];
    for (int i = 0; i < 3;  i++) e.ext[i]  = target[i + 8];
    e.attr = ATTR_ARCH;
    e.first_cluster = 0;
    e.file_size = 0;

    if (!dir_write_entry(g_root_lba, dir_cluster, idx, &e)) return -1;

    for (int fd = 0; fd < FAT12_MAX_FDS; fd++) {
        if (!g_fds[fd].used) {
            g_fds[fd].used          = true;
            g_fds[fd].first_cluster = 0;
            g_fds[fd].size          = 0;
            g_fds[fd].pos           = 0;
            g_fds[fd].parent_cluster = dir_cluster;
            g_fds[fd].dir_idx       = idx;
            return fd;
        }
    }
    return -1;
}

// ============================================================
// Read
// ============================================================

int fat12_read(int fd, void* buf, uint32_t size) {
    if (fd < 0 || fd >= FAT12_MAX_FDS || !g_fds[fd].used) return -1;
    FD& f = g_fds[fd];

    if (f.pos >= f.size) return 0;
    uint32_t left = f.size - f.pos;
    if (size > left) size = left;
    if (size == 0 || f.first_cluster == 0) return 0;

    uint8_t* dst = (uint8_t*)buf;
    uint32_t done = 0;
    uint32_t csz = cluster_size();

    // Navigate to the cluster containing f.pos
    uint16_t clus = f.first_cluster;
    uint32_t skip = f.pos / csz;
    for (uint32_t i = 0; i < skip; i++) {
        clus = fat_get(clus);
        if (clus >= FAT12_EOC) return (int)done;
    }

    uint32_t off_in_clus = f.pos % csz;

    while (done < size && clus >= 2 && clus < FAT12_EOC) {
        uint32_t clba = cluster_lba(clus);

        for (uint32_t s = 0; s < g_sec_per_clus && done < size; s++) {
            if (off_in_clus >= 512) {
                off_in_clus -= 512;
                continue;
            }
            read_sector(clba + s);
            uint32_t sec_start = off_in_clus;
            off_in_clus = 0;
            uint32_t avail = 512 - sec_start;
            uint32_t take = (size - done < avail) ? size - done : avail;
            for (uint32_t i = 0; i < take; i++)
                dst[done++] = g_buf[sec_start + i];
        }

        if (done < size) {
            uint16_t next = fat_get(clus);
            if (next >= FAT12_EOC) break;
            clus = next;
        }
    }

    f.pos += done;
    return (int)done;
}

// ============================================================
// Write
// ============================================================

int fat12_write(int fd, const void* buf, uint32_t size) {
    if (fd < 0 || fd >= FAT12_MAX_FDS || !g_fds[fd].used) return -1;
    FD& f = g_fds[fd];
    if (size == 0) return 0;

    const uint8_t* src = (const uint8_t*)buf;
    uint32_t done = 0;
    uint32_t csz = cluster_size();

    // Allocate first cluster if needed
    if (f.first_cluster == 0) {
        uint16_t new_clus = fat_alloc();
        if (!new_clus) return -1;
        f.first_cluster = new_clus;
        // Zero the new cluster
        for (int i = 0; i < 512; i++) g_buf[i] = 0;
        uint32_t clba = cluster_lba(new_clus);
        for (uint32_t s = 0; s < g_sec_per_clus; s++)
            ata_write(clba + s, 1, (uint16_t*)g_buf);
    }

    // Navigate to cluster containing pos, extending chain as needed
    uint16_t clus = f.first_cluster;
    uint32_t skip = f.pos / csz;
    for (uint32_t i = 0; i < skip; i++) {
        uint16_t next = fat_get(clus);
        if (next >= FAT12_EOC) {
            next = fat_alloc();
            if (!next) return (int)done;
            fat_set(clus, next);
            // Zero new cluster
            for (int j = 0; j < 512; j++) g_buf[j] = 0;
            uint32_t clba = cluster_lba(next);
            for (uint32_t s = 0; s < g_sec_per_clus; s++)
                ata_write(clba + s, 1, (uint16_t*)g_buf);
        }
        clus = (next >= FAT12_EOC) ? 0 : next;
        if (clus == 0) return (int)done;
    }

    uint32_t off_in_clus = f.pos % csz;

    while (done < size) {
        if (clus < 2 || clus >= FAT12_EOC) break;

        uint32_t clba = cluster_lba(clus);
        for (uint32_t s = 0; s < g_sec_per_clus && done < size; s++) {
            if (off_in_clus >= 512) {
                off_in_clus -= 512;
                continue;
            }
            read_sector(clba + s);
            uint32_t sec_start = off_in_clus;
            off_in_clus = 0;
            uint32_t avail = 512 - sec_start;
            uint32_t take = (size - done < avail) ? size - done : avail;
            for (uint32_t i = 0; i < take; i++)
                g_buf[sec_start + i] = src[done++];
            write_sector(clba + s);
        }

        if (done < size) {
            uint16_t next = fat_get(clus);
            if (next >= FAT12_EOC) {
                next = fat_alloc();
                if (!next) break;
                fat_set(clus, next);
                for (int j = 0; j < 512; j++) g_buf[j] = 0;
                uint32_t clba2 = cluster_lba(next);
                for (uint32_t ss = 0; ss < g_sec_per_clus; ss++)
                    ata_write(clba2 + ss, 1, (uint16_t*)g_buf);
            }
            clus = next;
        }
    }

    f.pos += done;
    if (f.pos > f.size) f.size = f.pos;
    return (int)done;
}

// ============================================================
// Flush — write size + first_cluster back to directory entry
// ============================================================

void fat12_flush(int fd) {
    if (fd < 0 || fd >= FAT12_MAX_FDS || !g_fds[fd].used) return;
    FD& f = g_fds[fd];

    DirEntry e;
    if (!dir_read_entry(g_root_lba, f.parent_cluster, f.dir_idx, &e)) return;

    e.file_size = f.size;
    e.first_cluster = f.first_cluster;

    dir_write_entry(g_root_lba, f.parent_cluster, f.dir_idx, &e);
}

// ============================================================
// Close
// ============================================================

void fat12_close(int fd) {
    if (fd < 0 || fd >= FAT12_MAX_FDS) return;
    fat12_flush(fd);
    g_fds[fd].used = false;
}

// ============================================================
// Size
// ============================================================

int fat12_size(int fd) {
    if (fd < 0 || fd >= FAT12_MAX_FDS || !g_fds[fd].used) return -1;
    return (int)g_fds[fd].size;
}

// ============================================================
// Delete
// ============================================================

bool fat12_delete(const char* path) {
    uint16_t dir_cluster;
    char target[11];
    if (!resolve_path(path, &dir_cluster, target)) return false;
    if (target[0] == 0) return false;

    DirEntry e;
    int idx = find_in_dir(dir_cluster, target, &e, nullptr);
    if (idx < 0) return false;
    if (e.attr & ATTR_DIR) return false;

    // Free cluster chain
    if (e.first_cluster >= 2)
        fat_free_chain(e.first_cluster);

    // Mark directory entry as deleted
    e.name[0] = (char)0xE5;

    // Re-derive directory entry location
    if (dir_cluster == 0) {
        dir_write_entry(g_root_lba, dir_cluster, idx, &e);
    } else {
        dir_write_entry(g_root_lba, dir_cluster, idx, &e);
    }
    return true;
}

// ============================================================
// Rename
// ============================================================

bool fat12_rename(const char* old_path, const char* new_path) {
    uint16_t old_dir;
    char old_83[11];
    if (!resolve_path(old_path, &old_dir, old_83)) return false;
    if (old_83[0] == 0) return false;

    DirEntry e;
    int idx = find_in_dir(old_dir, old_83, &e, nullptr);
    if (idx < 0) return false;

    uint16_t new_dir;
    char new_83[11];
    if (!resolve_path(new_path, &new_dir, new_83)) return false;
    if (new_83[0] == 0) return false;

    // Check new name doesn't already exist
    if (find_in_dir(new_dir, new_83, nullptr, nullptr) >= 0) return false;

    // Update the 8.3 name in the entry
    for (int i = 0; i < 8;  i++) e.name[i] = new_83[i];
    for (int i = 0; i < 3;  i++) e.ext[i]  = new_83[i + 8];

    // Write back to OLD directory position (entry still at same slot)
    dir_write_entry(g_root_lba, old_dir, idx, &e);

    // If old and new are in different directories, we'd need to move.
    // For simplicity, we only support rename within the same directory.
    return true;
}

// ============================================================
// Mkdir
// ============================================================

bool fat12_mkdir(const char* path) {
    uint16_t dir_cluster;
    char target[11];
    if (!resolve_path(path, &dir_cluster, target)) return false;
    if (target[0] == 0) return false;

    // Check if already exists
    if (find_in_dir(dir_cluster, target, nullptr, nullptr) >= 0) return false;

    // Allocate a cluster for the new directory
    uint16_t new_clus = fat_alloc();
    if (!new_clus) return false;

    // Zero the new cluster (empty directory)
    for (int i = 0; i < 512; i++) g_buf[i] = 0;
    uint32_t clba = cluster_lba(new_clus);
    for (uint32_t s = 0; s < g_sec_per_clus; s++)
        ata_write(clba + s, 1, (uint16_t*)g_buf);

    // Create the directory entry in the parent
    int idx = alloc_dir_entry(dir_cluster);
    if (idx < 0) {
        fat_set(new_clus, FAT12_FREE);
        return false;
    }

    DirEntry e = {};
    for (int i = 0; i < 8;  i++) e.name[i] = target[i];
    for (int i = 0; i < 3;  i++) e.ext[i]  = target[i + 8];
    e.attr = ATTR_DIR;
    e.first_cluster = new_clus;
    e.file_size = 0;

    return dir_write_entry(g_root_lba, dir_cluster, idx, &e);
}

// ============================================================
// Rmdir
// ============================================================

bool fat12_rmdir(const char* path) {
    uint16_t dir_cluster;
    char target[11];
    if (!resolve_path(path, &dir_cluster, target)) return false;
    if (target[0] == 0) return false;

    DirEntry e;
    int idx = find_in_dir(dir_cluster, target, &e, nullptr);
    if (idx < 0) return false;
    if (!(e.attr & ATTR_DIR)) return false;

    // Check that the directory is empty
    DirEntry child;
    uint32_t max_ents = (e.first_cluster == 0) ? g_root_entries : 10000;
    for (int i = 0; i < (int)max_ents; i++) {
        if (!dir_read_entry(g_root_lba, e.first_cluster, i, &child)) break;
        if ((uint8_t)child.name[0] == 0x00) break;
        if ((uint8_t)child.name[0] == 0xE5) continue;
        if (child.attr & ATTR_LFN) continue;
        // . and .. entries
        if (child.name[0] == '.' && (child.name[1] == ' ' || (child.name[1] == '.' && child.name[2] == ' ')))
            continue;
        return false;  // not empty
    }

    // Free the directory's cluster chain
    if (e.first_cluster >= 2)
        fat_free_chain(e.first_cluster);

    // Mark entry as deleted
    e.name[0] = (char)0xE5;
    return dir_write_entry(g_root_lba, dir_cluster, idx, &e);
}

// ============================================================
// Ls
// ============================================================

bool fat12_ls(const char* path,
              void (*cb)(const char* name, uint32_t size, bool is_dir))
{
    uint16_t dir_cluster = 0;

    if (path && path[0] != 0 && !(path[0] == '/' && path[1] == 0)) {
        uint16_t dc;
        char fn[11];
        if (!resolve_path(path, &dc, fn)) return false;
        if (fn[0] != 0) {
            DirEntry e;
            if (!find_in_dir(dc, fn, &e, nullptr)) return false;
            if (!(e.attr & ATTR_DIR)) return false;
            dir_cluster = e.first_cluster;
        } else {
            dir_cluster = dc;
        }
    }

    DirEntry e;
    uint32_t max_ents = (dir_cluster == 0) ? g_root_entries : 10000;
    for (int i = 0; i < (int)max_ents; i++) {
        if (!dir_read_entry(g_root_lba, dir_cluster, i, &e)) break;
        if ((uint8_t)e.name[0] == 0x00) break;
        if ((uint8_t)e.name[0] == 0xE5) continue;
        if (e.attr & ATTR_LFN) continue;

        // Skip . and .. in subdirectories
        if (e.name[0] == '.' && (e.name[1] == ' ' || (e.name[1] == '.' && e.name[2] == ' ')))
            continue;

        char display[13];
        from_83(e.name, display);
        cb(display, e.file_size, (e.attr & ATTR_DIR) != 0);
    }
    return true;
}
