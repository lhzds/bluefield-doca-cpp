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
    doca_buf *m_inner{};
    char *m_mem{};
    const size_t m_size{};

    Buffer(doca_buf *inner, char *mem, size_t size)
        :m_inner{inner}, m_mem{mem}, m_size{size} {}

    ~Buffer() {
        if (not m_inner) {
            return;
        }
        auto result = doca_buf_dec_refcount(m_inner, nullptr);
        if (result != DOCA_SUCCESS) {
            std::cerr << "Failed to dec refcount of buf: "
                      << doca_error_get_descr(result) << std::endl;
            return;
        }
    }

    void set_doca_buf_data(size_t size) {
        check_derr(
            doca_buf_set_data(m_inner, m_mem, size),
            "failed to set data of buf");
    }

    void clear_doca_buf_data() {
        check_derr(
            doca_buf_set_data(m_inner, m_mem, 0),
            "failed to clear data of buf");
    }

    std::string_view get_doca_buf_data() {
        void *data{};
        size_t len{};
        check_derr(doca_buf_get_data(m_inner, &data), "failed to get data of buf");
        check_derr(
            doca_buf_get_data_len(m_inner, &len), "failed to get data len of buf");
        return {(char *)data, len};
    }
};

class BufInventory {
public:
    doca_mmap *m_mmap{};
    char *m_mem;
    const size_t m_size{};

    BufInventory(doca_dev *dev, char *mem, size_t size, size_t max_bufs)
        : m_size{size}, m_mem{mem} {
        check_derr(
            doca_buf_inventory_create(max_bufs, &m_inner), "failed to create buf inv");

        check_derr(
            doca_buf_inventory_start(m_inner), "failed to start buf inv");
        check_derr(doca_mmap_create(&m_mmap), "failed to create mmap");
        check_derr(doca_mmap_add_dev(m_mmap, dev), "failed to add dev to mmap");
        check_derr(
            doca_mmap_set_permissions(m_mmap, DOCA_ACCESS_FLAG_PCI_READ_WRITE),
            "failed to set permissions of mmap");
        check_derr(
            doca_mmap_set_memrange(m_mmap, m_mem, m_size),
            "failed set memrange of mmap");
        check_derr(doca_mmap_start(m_mmap), "failed to start mmap");;
    }

    BufInventory(doca_dev *dev, size_t size, size_t max_bufs):
        BufInventory(dev, new char[size]{}, size, max_bufs)  {
        m_is_owner = true;
    }

    ~BufInventory() {
        auto result = doca_mmap_destroy(m_mmap);
        if (result != DOCA_SUCCESS) {
            std::cerr << "Failed to destroy mmap: "
                      << doca_error_get_descr(result) << std::endl;
            return;
        }

        result = doca_buf_inventory_destroy(m_inner);
        if (result != DOCA_SUCCESS) {
            std::cerr << "Failed to destroy inventory: "
                      << doca_error_get_descr(result) << std::endl;
            return;
        }

        if (m_is_owner) {
            delete[] m_mem;
        }
    }

    std::unique_ptr<Buffer> get_buf(size_t offset, size_t size) {
        doca_buf *buf{};
        check_derr(doca_buf_inventory_buf_get_by_addr(m_inner, m_mmap, m_mem + offset, size, &buf),
            "failed to get doca buf");
        return std::make_unique<Buffer>(buf, m_mem + offset, size);
    }

private:
    bool m_is_owner{};
    doca_buf_inventory *m_inner{};
};
}
