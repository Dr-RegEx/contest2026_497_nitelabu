#include "PackageStage.h"

#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <new>

namespace os {
namespace pm {
namespace {

class OwnedFd {
public:
    explicit OwnedFd(int fd) : mFd(fd) {}
    ~OwnedFd() { if (mFd >= 0) close(mFd); }
    OwnedFd(const OwnedFd &) = delete;
    OwnedFd &operator=(const OwnedFd &) = delete;
    int get() const { return mFd; }
    int release() { int fd = mFd; mFd = -1; return fd; }
private:
    int mFd;
};

int copySnapshot(int source, int destination, off_t size) {
    std::array<char, 1024> buffer;
    while (size > 0) {
        auto bytes = read(source, buffer.data(),
                          static_cast<size_t>(std::min<off_t>(size, buffer.size())));
        if (bytes < 0) {
            if (errno == EINTR) continue;
            return -errno;
        }
        if (bytes == 0) return -EIO;
        ssize_t written = 0;
        while (written < bytes) {
            auto count = write(destination, buffer.data() + written, bytes - written);
            if (count < 0) {
                if (errno == EINTR) continue;
                return -errno;
            }
            if (count == 0) return -EIO;
            written += count;
        }
        size -= bytes;
    }
    ssize_t extra;
    do { extra = read(source, buffer.data(), 1); } while (extra < 0 && errno == EINTR);
    return extra < 0 ? -errno : (extra == 0 ? 0 : -EAGAIN);
}

int removeStageTree(const std::string &path) {
    struct stat info;
    if (lstat(path.c_str(), &info) < 0) return errno == ENOENT ? 0 : -errno;
    if (!S_ISDIR(info.st_mode)) return unlink(path.c_str()) < 0 ? -errno : 0;
    DIR *directory = opendir(path.c_str());
    if (!directory) return -errno;
    int result = 0;
    for (;;) {
        errno = 0;
        auto *entry = readdir(directory);
        if (!entry) { if (errno) result = -errno; break; }
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        result = removeStageTree(path + "/" + name);
        if (result) break;
    }
    // NuttX tmpfs refuses rmdir while a directory reference is still open.
    // Close the iterator before removing the directory, as PM's existing
    // removeDirectory helper does. lstat also prevents following symlinks.
    if (closedir(directory) < 0 && !result) result = -errno;
    if (!result && rmdir(path.c_str()) < 0) result = -errno;
    return result;
}

} // namespace

PackageStage::~PackageStage() {
    int error = removeStageTree(mDirectory);
    if (error) std::fprintf(stderr, "PMS package staging cleanup failed: %d\n", error);
}

std::shared_ptr<PackageStage> PackageStage::create(const std::string &source,
                                                 const std::string &base, int &error) {
    error = -EINVAL;
    if (source.empty() || source.find('\0') != std::string::npos || base.empty() ||
        base.find('\0') != std::string::npos) return nullptr;

    OwnedFd input(open(source.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
    if (input.get() < 0) { error = -errno; return nullptr; }
    struct stat info;
    if (fstat(input.get(), &info) < 0) { error = -errno; return nullptr; }
    if (!S_ISREG(info.st_mode) || info.st_size <= 0) return nullptr;

    std::error_code ec;
    std::filesystem::create_directories(base, ec);
    if (ec) { error = -ec.value(); return nullptr; }
    std::string directory = base + "/pm-XXXXXX";
    if (!mkdtemp(directory.data())) { error = -errno; return nullptr; }
    auto *owned = new (std::nothrow) PackageStage(directory);
    if (!owned) { rmdir(directory.c_str()); error = -ENOMEM; return nullptr; }
    auto stage = std::shared_ptr<PackageStage>(owned);
    // The one initial write descriptor is closed before verification. No
    // caller-provided path is reopened after creating this private copy.
    OwnedFd output(open(stage->path().c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR));
    if (output.get() < 0) { error = -errno; return nullptr; }
    error = copySnapshot(input.get(), output.get(), info.st_size);
    if (error) return nullptr;
    struct stat after;
    if (fstat(input.get(), &after) < 0) { error = -errno; return nullptr; }
    if (after.st_size != info.st_size) { error = -EAGAIN; return nullptr; }
    if (close(output.release()) < 0) { error = -errno; return nullptr; }
    error = 0;
    return stage;
}

} // namespace pm
} // namespace os
