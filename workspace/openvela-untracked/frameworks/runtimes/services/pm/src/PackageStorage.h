/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <limits>
#include <sys/stat.h>
#include <sys/statvfs.h>

#if defined(__NuttX__) && defined(CONFIG_BUILD_FLAT) && defined(CONFIG_FS_TMPFS) && \
        CONFIG_FS_HEAPSIZE == 0 && !defined(CONFIG_MM_KERNEL_HEAP)
#include <malloc.h>
#include <sys/statfs.h>
#define PMS_SHARED_TMPFS_HEAP 1
#endif

namespace os {
namespace pm {

/* This is an admission estimate, not a reservation or an extraction quota.
 * Allocation and I/O failures must still fail installation and clean up.
 */
static inline bool hasEnoughDiskSpace(const char *diskPath, const char *packagePath) {
    struct stat packageStat;
    struct statvfs diskStat;
    if (stat(packagePath, &packageStat) != 0 || packageStat.st_size < 0 ||
        statvfs(diskPath, &diskStat) != 0) {
        return false;
    }

    uint64_t unit = diskStat.f_frsize ? diskStat.f_frsize : diskStat.f_bsize;
    uint64_t blocks = diskStat.f_bavail;
    if (unit == 0 || blocks > std::numeric_limits<uint64_t>::max() / unit) {
        return false;
    }
    uint64_t available = blocks * unit;

#ifdef PMS_SHARED_TMPFS_HEAP
    struct statfs fsInfo;
    if (statfs(diskPath, &fsInfo) != 0) {
        return false;
    }
    if (fsInfo.f_type == TMPFS_MAGIC) {
        /* NuttX tmpfs reports slack in already allocated objects, not free
         * backing capacity. Only in this shared-heap FLAT configuration does
         * mallinfo describe the heap used by fs_heap_malloc. Do not apply it
         * to a kernel/dedicated filesystem heap or add slack a second time.
         */
        const struct mallinfo heap = mallinfo();
        const uint64_t free = heap.fordblks;
#ifdef CONFIG_SYSTEM_PACKAGE_SERVICE_TMPFS_RESERVE
        const uint64_t reserve = CONFIG_SYSTEM_PACKAGE_SERVICE_TMPFS_RESERVE;
#else
        const uint64_t reserve = 0;
#endif
        available = free > reserve ? free - reserve : 0;
    }
#endif

    /* Preserve the existing three-times-package estimate without overflow. */
    return static_cast<uint64_t>(packageStat.st_size) <= available / 3;
}

} // namespace pm
} // namespace os

#undef PMS_SHARED_TMPFS_HEAP
