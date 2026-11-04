#include "PackageList.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "PackageUtils.h"

namespace os {
namespace pm {

int writePackageList(const std::string &path, const std::string &data) {
    if (path.empty() || path.find('\0') != std::string::npos ||
        std::filesystem::path(path).filename().empty()) return -EINVAL;
    auto parent = std::filesystem::path(path).parent_path();
    if (parent.empty()) parent = ".";
    struct stat info;
    if (lstat(parent.c_str(), &info) < 0) {
        if (errno != ENOENT) return -errno;
        if (!createDirectory(parent.c_str())) return -EACCES;
    }
    bool hadOld = lstat(path.c_str(), &info) == 0;
    if (!hadOld && errno != ENOENT) return -errno;
    if (hadOld && !S_ISREG(info.st_mode)) return -EINVAL;

    auto directory = (parent / ".pm-list-XXXXXX").string();
    if (!mkdtemp(directory.data())) return -errno;
    const auto next = joinPath(directory, "next");
    const auto previous = joinPath(directory, "previous");
    auto cleanup = [&] {
        if (!removeDirectory(directory.c_str()))
            std::fprintf(stderr, "PMS registry cleanup retained: %s\n", directory.c_str());
    };
    int fd;
    do { fd = open(next.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600); }
    while (fd < 0 && errno == EINTR);
    if (fd < 0) { const int error = -errno; cleanup(); return error; }
    int error = 0;
    size_t offset = 0;
    while (offset < data.size()) {
        const auto written = write(fd, data.data() + offset, data.size() - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { error = written < 0 ? -errno : -EIO; break; }
        offset += static_cast<size_t>(written);
    }
    if (!error) {
        int result;
        do { result = fsync(fd); } while (result < 0 && errno == EINTR);
        if (result < 0) error = -errno;
    }
    // Do not retry close after EINTR: its descriptor may already be released.
    if (close(fd) < 0 && !error) error = -errno;
    if (error) { cleanup(); return error; }

    if (hadOld && rename(path.c_str(), previous.c_str()) < 0) {
        error = -errno; cleanup(); return error;
    }
    if (rename(next.c_str(), path.c_str()) < 0) {
        error = -errno;
        if (hadOld && rename(previous.c_str(), path.c_str()) < 0) {
            std::fprintf(stderr, "PMS registry rollback failed: old list retained at %s (%d)\n",
                         previous.c_str(), errno);
            return -EIO;
        }
        cleanup();
        return error;
    }
    cleanup();
    return 0;
}

} // namespace pm
} // namespace os
