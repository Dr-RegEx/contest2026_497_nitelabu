#pragma once

#include <memory>
#include <string>

namespace os {
namespace pm {

// One owned snapshot for both manifest admission and full extraction. This
// isolates changes to the caller's file; it is not a sandbox against a peer
// with unrestricted access to the same filesystem (notably FLAT/root).
class PackageStage {
public:
    static std::shared_ptr<PackageStage> create(const std::string &source,
                                               const std::string &base, int &error);
    ~PackageStage();
    PackageStage(const PackageStage &) = delete;
    PackageStage &operator=(const PackageStage &) = delete;
    const std::string &path() const { return mPath; }
    const std::string &directory() const { return mDirectory; }

private:
    explicit PackageStage(std::string directory)
          : mDirectory(std::move(directory)), mPath(mDirectory + "/package.rpk") {}
    std::string mDirectory;
    std::string mPath;
};

} // namespace pm
} // namespace os
