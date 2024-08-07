#pragma once

#include <cstddef>
#include <stdexcept>
#include <iostream>
#include <utility>

class PartIterator {
public:
    PartIterator(size_t total_size, size_t part_size)
        :total_size_{total_size}, part_size_{part_size}, next_part_{} {}
    std::pair<size_t, size_t> next() {
        if (not has_next()) {
            throw std::out_of_range("no more part");
        }
        auto offset = next_part_ * part_size_;
        ++next_part_;
        auto len = (total_size_ - offset) < part_size_ ? total_size_ - offset : part_size_;
        return {offset, len};
    }
    bool has_next() const {
        return next_part_ * part_size_ < total_size_;
    }

private:
    size_t total_size_{};
    size_t part_size_{};
    size_t next_part_{};
};
