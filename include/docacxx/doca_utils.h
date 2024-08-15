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
#include <stdexcept>

namespace docacxx {
template<class... Args>
static inline void throw_with(std::string_view format, Args... args) {
    throw std::runtime_error(fmt::format(fmt::runtime(format), args...));
}

static inline void check_derr(
    doca_error_t doca_error,
    const char *message) {
    if (doca_error != DOCA_ERROR_IN_PROGRESS && doca_error != DOCA_SUCCESS) [[unlikely]] {
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
}
