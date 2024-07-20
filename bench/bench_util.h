#pragma once
#include <string>

constexpr size_t KB{1 << 10};
constexpr size_t MB{1 << 20};
constexpr size_t GB{1 << 30};

inline static std::string size_str(size_t sz) {
    if (sz >= 1 * MB) {
        return std::to_string(sz / (1 * MB)) + "M";
    }
    if (sz >= 1 * KB) {
        return std::to_string(sz / (1 * KB)) + "K";
    }
    return std::to_string(sz);
}
