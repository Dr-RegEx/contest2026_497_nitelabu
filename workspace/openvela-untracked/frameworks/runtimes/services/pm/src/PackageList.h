#pragma once

#include <string>

namespace os {
namespace pm {

// Write and check a separate file before replacing packages.list. Keep the old
// file until publication succeeds, including on tmpfs (rename cannot replace
// an existing name there). Does not promise process/power-cut atomicity.
int writePackageList(const std::string &path, const std::string &data);

} // namespace pm
} // namespace os
