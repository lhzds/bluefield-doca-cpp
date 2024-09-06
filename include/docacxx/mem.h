#pragma once

#include <doca_compress.h>
#include <iostream>
#include <memory>
#include <doca_mmap.h>
#include <doca_buf.h>
#include "doca_utils.h"

namespace docacxx {
class Buffer {
public:
    doca_buf *buf{};
    char *mem{};
    const size_t len{};

    Buffer(doca_buf *buffer, char *memory, size_t length)
        :buf{buffer}, mem{memory}, len{length} {}

    ~Buffer() {
        if (not buf) {
            return;
        }
        auto result = doca_buf_dec_refcount(buf, nullptr);
        if (result != DOCA_SUCCESS) {
            std::cerr << "Failed to dec refcount of buf: "
                      << doca_error_get_descr(result) << std::endl;
            return;
        }
    }

    void set_doca_buf_data(size_t length) {
        check_derr(
            doca_buf_set_data(buf, mem, length),
            "failed to set data of buf");
    }

    void clear_doca_buf_data() {
        check_derr(
            doca_buf_set_data(buf, mem, 0),
            "failed to clear data of buf");
    }

    std::string_view get_doca_buf_data() {
        void *data{};
        size_t len{};
        check_derr(doca_buf_get_data(buf, &data), "failed to get data of buf");
        check_derr(
            doca_buf_get_data_len(buf, &len), "failed to get data len of buf");
        return {(char *)data, len};
    }
};

class BufInventory {
public:
    doca_mmap *mmap{};
    char *mem;
    const size_t len{};

    BufInventory(doca_dev *dev, char *memory, size_t length, size_t max_bufs)
        : len{length}, mem{memory} {
        check_derr(
            doca_buf_inventory_create(max_bufs, &buf_inv_), "failed to create buf inv");

        check_derr(
            doca_buf_inventory_start(buf_inv_), "failed to start buf inv");
        check_derr(doca_mmap_create(&mmap), "failed to create mmap");
        check_derr(doca_mmap_add_dev(mmap, dev), "failed to add dev to mmap");
        check_derr(
            doca_mmap_set_permissions(mmap, DOCA_ACCESS_FLAG_PCI_READ_WRITE),
            "failed to set permissions of mmap");
        check_derr(
            doca_mmap_set_memrange(mmap, mem, len),
            "failed set memrange of mmap");
        check_derr(doca_mmap_start(mmap), "failed to start mmap");;
    }

    BufInventory(doca_dev *dev, size_t length, size_t max_bufs):
        BufInventory(dev, new char[length]{}, length, max_bufs)  {
        is_owner_ = true;
    }

    ~BufInventory() {
        auto result = doca_mmap_destroy(mmap);
        if (result != DOCA_SUCCESS) {
            std::cerr << "Failed to destroy mmap: "
                      << doca_error_get_descr(result) << std::endl;
            return;
        }

        result = doca_buf_inventory_destroy(buf_inv_);
        if (result != DOCA_SUCCESS) {
            std::cerr << "Failed to destroy inventory: "
                      << doca_error_get_descr(result) << std::endl;
            return;
        }

        if (is_owner_) {
            delete[] mem;
        }
    }

    std::unique_ptr<Buffer> get_buf(size_t offset, size_t length) {
        doca_buf *buf{};
        check_derr(doca_buf_inventory_buf_get_by_addr(buf_inv_, mmap, mem + offset, length, &buf),
            "failed to get doca buf");
        return std::make_unique<Buffer>(buf, mem + offset, length);
    }

private:
    bool is_owner_{};
    doca_buf_inventory *buf_inv_{};
};
}
