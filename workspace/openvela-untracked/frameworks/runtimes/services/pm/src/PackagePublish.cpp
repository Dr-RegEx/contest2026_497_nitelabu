#include "PackagePublish.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "PackageName.h"
#include "PackageUtils.h"

namespace os {
namespace pm {
namespace {

void removeEmptyBackup(const std::string &directory) {
    if (rmdir(directory.c_str()) < 0) {
        std::fprintf(stderr, "PMS publication empty backup retained: %s (%d)\n",
                     directory.c_str(), errno);
    }
}

} // namespace

int publishPackageDirectory(const std::string &unpacked, const std::string &installedRoot,
                            const std::string &packageName) {
    return publishPackageDirectory(unpacked, installedRoot, packageName, [] { return 0; });
}

int publishPackageDirectory(const std::string &unpacked, const std::string &installedRoot,
                            const std::string &packageName, const std::function<int()> &commit) {
    if (unpacked.empty() || installedRoot.empty() ||
        unpacked.find('\0') != std::string::npos ||
        installedRoot.find('\0') != std::string::npos ||
        !isValidPackageName(packageName) || !commit) return -EINVAL;

    struct stat info;
    if (lstat(unpacked.c_str(), &info) < 0) return -errno;
    if (!S_ISDIR(info.st_mode)) return -ENOTDIR;

    const auto destination = joinPath(installedRoot, packageName);
    if (unpacked == destination) return -EINVAL;
    if (lstat(destination.c_str(), &info) < 0) {
        if (errno != ENOENT) return -errno;
        if (rename(unpacked.c_str(), destination.c_str()) < 0) return -errno;
        const int error = commit();
        if (error && rename(destination.c_str(), unpacked.c_str()) < 0) {
            std::fprintf(stderr, "PMS new publication rollback failed: %s (%d), commit %d\n",
                         destination.c_str(), errno, error);
            return -EIO;
        }
        return error;
    }
    // Do not replace an unexpected file or follow a symlink as an old app.
    if (!S_ISDIR(info.st_mode)) return -ENOTDIR;

    // Keep the recovery copy in the installed filesystem, outside the owned
    // input stage: stage destruction must never erase an unrecovered old app.
    auto backup = joinPath(installedRoot, ".pm-XXXXXX");
    if (!mkdtemp(backup.data())) return -errno;
    const auto previous = joinPath(backup, "previous");
    if (rename(destination.c_str(), previous.c_str()) < 0) {
        const int error = -errno;
        removeEmptyBackup(backup);
        return error;
    }

    if (rename(unpacked.c_str(), destination.c_str()) < 0) {
        const int error = -errno;
        if (rename(previous.c_str(), destination.c_str()) < 0) {
            const int restoreError = errno;
            std::fprintf(stderr,
                         "PMS publication rollback failed: %s -> %s (%d); "
                         "old package retained at %s, publish error %d\n",
                         previous.c_str(), destination.c_str(), restoreError,
                         previous.c_str(), error);
            return -EIO;
        }
        removeEmptyBackup(backup);
        return error;
    }

    const int error = commit();
    if (error) {
        if (rename(destination.c_str(), unpacked.c_str()) < 0 ||
            rename(previous.c_str(), destination.c_str()) < 0) {
            std::fprintf(stderr,
                         "PMS commit rollback failed: old package retained at %s (%d), commit %d\n",
                         previous.c_str(), errno, error);
            return -EIO;
        }
        removeEmptyBackup(backup);
        return error;
    }

    if (!removeDirectory(backup.c_str())) {
        // Publication has succeeded. Preserve the leftover backup and report
        // it rather than roll back an installation already visible to readers.
        std::fprintf(stderr, "PMS publication backup cleanup failed: %s\n", backup.c_str());
    }
    return 0;
}

int removePackageDirectories(const std::vector<std::string> &paths,
                             const std::function<int()> &commit) {
    if (paths.empty() || !commit) return -EINVAL;
    struct Saved { std::string original, directory, previous; };
    std::vector<Saved> saved;
    saved.reserve(paths.size());
    int error = 0;
    for (size_t i = 0; i < paths.size(); ++i) {
        auto path = paths[i];
        if (path.empty() || path.find('\0') != std::string::npos) { error = -EINVAL; break; }
        while (path.size() > 1 && path.back() == '/') path.pop_back();
        const auto filename = std::filesystem::path(path).filename();
        if (filename.empty() || filename == "." || filename == "..") { error = -EINVAL; break; }
        struct stat info;
        if (lstat(path.c_str(), &info) < 0) {
            if (errno == ENOENT && i != 0) continue;
            error = -errno; break;
        }
        if (!S_ISDIR(info.st_mode)) { error = -ENOTDIR; break; }
        auto directory = (std::filesystem::path(path).parent_path() / ".pm-remove-XXXXXX").string();
        if (!mkdtemp(directory.data())) { error = -errno; break; }
        Saved entry{path, directory, joinPath(directory, "previous")};
        if (rename(path.c_str(), entry.previous.c_str()) < 0) {
            error = -errno; removeEmptyBackup(directory); break;
        }
        saved.push_back(std::move(entry));
    }
    if (!error) error = commit();
    if (error) {
        bool restored = true;
        for (auto it = saved.rbegin(); it != saved.rend(); ++it) {
            if (rename(it->previous.c_str(), it->original.c_str()) < 0) {
                std::fprintf(stderr, "PMS removal rollback failed: retained at %s (%d)\n",
                             it->previous.c_str(), errno);
                restored = false;
            } else removeEmptyBackup(it->directory);
        }
        return restored ? error : -EIO;
    }
    for (const auto &entry : saved) {
        if (!removeDirectory(entry.directory.c_str()))
            std::fprintf(stderr, "PMS removal cleanup retained: %s\n", entry.directory.c_str());
    }
    return 0;
}

} // namespace pm
} // namespace os
