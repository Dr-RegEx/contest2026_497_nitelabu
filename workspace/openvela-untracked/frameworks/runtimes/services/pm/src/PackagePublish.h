#pragma once

#include <string>
#include <functional>
#include <vector>

namespace os {
namespace pm {

// Publish an already verified directory. A failed replacement restores the
// old directory, or retains it in a named recovery directory if rollback
// itself fails. This is not a crash-atomic package/registry transaction.
int publishPackageDirectory(const std::string &unpacked, const std::string &installedRoot,
                            const std::string &packageName);

// Keep the old directory until commit succeeds. On a reported commit failure,
// move the new directory back to its owned stage and restore the old one.
// PM serializes these operations on its service loop. This is error recovery,
// not atomic visibility to arbitrary filesystem readers or power-cut recovery.
int publishPackageDirectory(const std::string &unpacked, const std::string &installedRoot,
                            const std::string &packageName, const std::function<int()> &commit);

// Quarantine each directory on its own filesystem before committing removal
// from the registry. Only after that commit may package/user data be deleted.
// The first directory is required; missing subsequent data directories are OK.
int removePackageDirectories(const std::vector<std::string> &paths,
                             const std::function<int()> &commit);

} // namespace pm
} // namespace os
