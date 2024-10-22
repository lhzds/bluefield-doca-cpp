#pragma once

#include <doca_buf_inventory.h>
#include <doca_compress.h>
#include <iostream>
#include <memory>
#include <doca_mmap.h>
#include <doca_buf.h>
#include "doca_utils.h"

namespace docacxx {
class Mmap {
public:
    doca_mmap *m_inner{};
    char *m_mem{};
    size_t m_size{};

    Mmap(doca_dev * dev, char *mem, size_t size, uint32_t access_mask)
        : m_mem(mem), m_size(size) {
        check_derr(doca_mmap_create(&m_inner), "failed to create mmap");
        check_derr(doca_mmap_add_dev(m_inner, dev), "failed to add dev to mmap");
        check_derr(doca_mmap_set_permissions(m_inner, access_mask),
            "failed to set permissions of mmap");
        check_derr(doca_mmap_set_memrange(m_inner, m_mem, m_size),
            "failed set memrange of mmap");
        check_derr(doca_mmap_start(m_inner), "failed to start mmap");
    }

    Mmap(doca_dev *dev, size_t size, uint32_t access_mask)
        : Mmap(dev, new char[size]{}, size, access_mask) {
        m_is_mem_owner = true;
    }

    Mmap(std::string_view export_desc, doca_dev *dev, char *remote_mem, size_t size)
        :m_mem(remote_mem), m_size(size) {
        doca_mmap_create_from_export(nullptr, export_desc.data(), export_desc.size(),
            dev, &m_inner);
    }
    
    ~Mmap() {
        auto result = doca_mmap_destroy(m_inner);
        if (result != DOCA_SUCCESS) {
            std::cerr << "Failed to destroy mmap: "
                      << doca_error_get_descr(result) << std::endl;
            return;
        }

        if (m_is_mem_owner) {
            delete[] m_mem;
        }
    }

    std::string_view export_pci(doca_dev *dev) {
        const void *export_desc{};
        size_t len{};
        check_derr(doca_mmap_export_pci(m_inner, dev, &export_desc, &len)
            , "failed to export mmap by pci");
        return {(const char *)export_desc, len};
    }

private:
    bool m_is_mem_owner{};
};

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
        check_derr(doca_buf_set_data(m_inner, m_mem, size),
            "failed to set data of buf");
    }

    void clear_doca_buf_data() {
        check_derr(doca_buf_set_data(m_inner, m_mem, 0),
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
    doca_buf_inventory *m_inner{};

    BufInventory(doca_dev *dev, char *mem, size_t size, size_t max_bufs,
        uint32_t access_mask = DOCA_ACCESS_FLAG_PCI_READ_WRITE) {
        check_derr(doca_buf_inventory_create(max_bufs, &m_inner),
            "failed to create buf inv");
        m_mmap = std::make_unique<Mmap>(dev, mem, size, access_mask);
    }

    BufInventory(doca_dev *dev, size_t size, size_t max_bufs,
        uint32_t access_mask = DOCA_ACCESS_FLAG_PCI_READ_WRITE) {
        check_derr(doca_buf_inventory_create(max_bufs, &m_inner),
            "failed to create buf inv");
        m_mmap = std::make_unique<Mmap>(dev, size, access_mask);
    }

    BufInventory(std::string_view export_desc, doca_dev *dev, char *remote_mem, size_t size, size_t max_bufs) {
        check_derr(doca_buf_inventory_create(max_bufs, &m_inner),
            "failed to create buf inv");
        m_mmap = std::make_unique<Mmap>(export_desc, dev, remote_mem, size);
    }

    ~BufInventory() {
        auto result = doca_buf_inventory_destroy(m_inner);
        if (result != DOCA_SUCCESS) {
            std::cerr << "Failed to destroy inventory: "
                      << doca_error_get_descr(result) << std::endl;
            return;
        }
    }

    std::unique_ptr<Buffer> get_buf(size_t offset, size_t size) {
        doca_buf *buf{};
        check_derr(doca_buf_inventory_buf_get_by_addr(m_inner, m_mmap->m_inner, data() + offset, size, &buf),
            "failed to get doca buf");
        return std::make_unique<Buffer>(buf, data() + offset, size);
    }

    size_t size() {
        return m_mmap->m_size;
    }

    char * data() {
        return m_mmap->m_mem;
    }

    doca_mmap *raw_mmap() {
        return m_mmap->m_inner;
    }

private:
    std::unique_ptr<Mmap> m_mmap;
};

class BufManager {
public:
    doca_buf_inventory *m_inner{};
    
   BufManager(doca_dev *dev, size_t max_bufs) {
        check_derr(doca_buf_inventory_create(max_bufs, &m_inner),
            "failed to create buf inv");
    }

    ~BufManager() {
        auto result = doca_buf_inventory_destroy(m_inner);
        if (result != DOCA_SUCCESS) {
            std::cerr << "Failed to destroy inventory: "
                      << doca_error_get_descr(result) << std::endl;
            return;
        }
    }

    std::unique_ptr<Buffer> get_buf(Mmap &mmap, size_t offset, size_t size) {
        doca_buf *buf{};
        check_derr(doca_buf_inventory_buf_get_by_addr(m_inner, mmap.m_inner, mmap.m_mem + offset, size, &buf),
            "failed to get doca buf");
        return std::make_unique<Buffer>(buf, mmap.m_mem + offset, size);
    }
};
}
