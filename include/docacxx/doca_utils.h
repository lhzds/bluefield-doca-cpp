#pragma once

#include <cstdint>
#include <cstring>
#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_comch_consumer.h>
#include <doca_comch_producer.h>
#include <doca_error.h>
#include <doca_dev.h>
#include <doca_mmap.h>
#include <fmt/core.h>
#include <iostream>
#include <stdexcept>
#include <vector>

template<class... Args>
static inline void throw_with(std::string_view format, Args... args) {
    throw std::runtime_error(fmt::format(fmt::runtime(format), args...));
}

static inline void check_derr(
    doca_error_t doca_error,
    const char *message,
    doca_error_t expected = DOCA_SUCCESS) {
    if (doca_error != expected) [[unlikely]] {
        throw_with(
            "[{}]{}: {}", doca_error_get_name(doca_error), message,
            doca_error_get_descr(doca_error));
    }
}

static inline doca_error_t
open_doca_device_with_pci(const char *pci_addr, struct doca_dev **retval) {
    struct doca_devinfo **dev_list{};
    uint32_t nb_devs{};
    uint8_t is_addr_equal{};
    doca_error_t res{};

    /* Set default return value */
    *retval = NULL;

    res = doca_devinfo_create_list(&dev_list, &nb_devs);
    if (res != DOCA_SUCCESS) { return res; }

    /* Search */
    for (int i = 0; i < nb_devs; i++) {
        res = doca_devinfo_is_equal_pci_addr(
            dev_list[i], pci_addr, &is_addr_equal);
        if (res == DOCA_SUCCESS && is_addr_equal) {
            /* if device can be opened */
            res = doca_dev_open(dev_list[i], retval);
            if (res == DOCA_SUCCESS) {
                doca_devinfo_destroy_list(dev_list);
                return res;
            }
        }
    }

    res = DOCA_ERROR_NOT_FOUND;
    doca_devinfo_destroy_list(dev_list);
    return res;
}

static inline doca_error_t open_doca_device_rep_with_pci(
    struct doca_dev *local,
    enum doca_devinfo_rep_filter filter,
    const char *pci_addr,
    struct doca_dev_rep **retval) {
    uint32_t nb_rdevs{};
    struct doca_devinfo_rep **rep_dev_list{};
    uint8_t is_addr_equal{};
    doca_error_t result{};

    *retval = NULL;

    /* Search */
    result =
        doca_devinfo_rep_create_list(local, filter, &rep_dev_list, &nb_rdevs);
    if (result != DOCA_SUCCESS) { return DOCA_ERROR_INVALID_VALUE; }

    for (int i = 0; i < nb_rdevs; i++) {
        result = doca_devinfo_rep_is_equal_pci_addr(
            rep_dev_list[i], pci_addr, &is_addr_equal);
        if (result == DOCA_SUCCESS && is_addr_equal
            && doca_dev_rep_open(rep_dev_list[i], retval) == DOCA_SUCCESS) {
            doca_devinfo_rep_destroy_list(rep_dev_list);
            return DOCA_SUCCESS;
        }
    }

    doca_devinfo_rep_destroy_list(rep_dev_list);
    return DOCA_ERROR_NOT_FOUND;
}

// single buf
struct SingleLocalDocaMem {
    std::vector<char> mem;
    doca_buf *buf{};
    doca_mmap *mmap{};
    doca_buf_inventory *buf_inv{};

    SingleLocalDocaMem(doca_dev *dev, size_t total_mem_size) {
        mem.resize(total_mem_size);
        check_derr(
            doca_buf_inventory_create(1, &buf_inv), "failed to create buf inv");

        check_derr(
            doca_buf_inventory_start(buf_inv), "failed to start buf inv");
        check_derr(doca_mmap_create(&mmap), "failed to create mmap");
        check_derr(doca_mmap_add_dev(mmap, dev), "failed to add dev to mmap");
        check_derr(
            doca_mmap_set_permissions(mmap, DOCA_ACCESS_FLAG_PCI_READ_WRITE),
            "failed to set permissions of mmap");
        check_derr(
            doca_mmap_set_memrange(mmap, mem.data(), total_mem_size),
            "failed set memrange of mmap");
        check_derr(doca_mmap_start(mmap), "failed to start mmap");
        check_derr(
            doca_buf_inventory_buf_get_by_addr(
                buf_inv, mmap, mem.data(), mem.size(), &buf),
            "failed to get doca buf");
    }

    ~SingleLocalDocaMem() {
        doca_error_t result{};

        result = doca_buf_dec_refcount(buf, nullptr);
        if (result != DOCA_SUCCESS) {
            std::cerr << "Failed to dec refcount of buf: "
                      << doca_error_get_descr(result) << std::endl;
            return;
        }

        result = doca_mmap_destroy(mmap);
        if (result != DOCA_SUCCESS) {
            std::cerr << "Failed to destroy mmap: "
                      << doca_error_get_descr(result) << std::endl;
            return;
        }

        result = doca_buf_inventory_destroy(buf_inv);
        if (result != DOCA_SUCCESS) {
            std::cerr << "Failed to destroy inventory: "
                      << doca_error_get_descr(result) << std::endl;
            return;
        }
    }

    void set_data(std::string_view src) {
        std::memcpy(mem.data(), src.data(), src.length());
        check_derr(
            doca_buf_set_data(buf, mem.data(), src.length()),
            "failed to set data of buf");
    }

    std::string_view get_data() {
        void *data{};
        size_t len{};
        check_derr(doca_buf_get_data(buf, &data), "failed to get data of buf");
        check_derr(
            doca_buf_get_data_len(buf, &len), "failed to get data len of buf");
        return {(char *)data, len};
    }

    void clear_data() {
        check_derr(
            doca_buf_set_data(buf, mem.data(), 0),
            "failed to clear data of buf");
    }
};
