#pragma once

#include <limits.h>
#include <string>

namespace os {
namespace pm {

// A package identifier becomes one directory component. Keep native names
// compatible without imposing a reverse-DNS naming convention.
inline bool isValidPackageName(const std::string &name) {
    if (name.empty() || name.size() > NAME_MAX || name.front() == '.' ||
        name == "packages.list" || name == "tmp") {
        return false;
    }
    for (unsigned char ch : name) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.')) {
            return false;
        }
    }
    return true;
}

} // namespace pm
} // namespace os
