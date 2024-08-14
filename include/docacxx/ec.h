#pragma once

#include "docacxx/context.h"
#include <doca_erasure_coding.h>
#include <functional>

namespace docacxx {
class Ec: public ContextBase {
public:
    using CustomEcCb = std::function<void(doca_data)>;

    struct Cfg {
        size_t create_task_num{};
        size_t update_task_num{};
        size_t recover_task_num{};
    };

    Ec(doca_pe *pe, doca_dev *dev, const Cfg &cfg);
    virtual ~Ec() = default;

    doca_ec *inner{};

    CustomEcCb custom_create_cb{[](doca_data) {}};
    CustomEcCb custom_create_error_cb{[](doca_data) {}};
    CustomEcCb custom_update_cb{[](doca_data) {}};
    CustomEcCb custom_update_error_cb{[](doca_data) {}};
    CustomEcCb custom_recover_cb{[](doca_data) {}};
    CustomEcCb custom_recover_error_cb{[](doca_data) {}};

    doca_ec_matrix *create_matrix(doca_ec_matrix_type type,
        size_t data_block_count, size_t rdnc_block_count);
    doca_ec_matrix *create_recover_matrix(const struct doca_ec_matrix *coding_matrix,
        std::vector<uint32_t> &missing_indices);
    void destroy_matrix(doca_ec_matrix *);

    doca_task *alloc_create_task(const doca_ec_matrix *matrix,
        const doca_buf *src, doca_buf *dst, doca_data task_user_data = doca_data{});
    doca_task *alloc_update_task(const doca_ec_matrix *matrix,
        const doca_buf *src, doca_buf *dst, doca_data task_user_data = doca_data{});
    doca_task *alloc_recover_task(const doca_ec_matrix *matrix,
        const doca_buf *src, doca_buf *dst, doca_data task_user_data = doca_data{});

protected:
    void clean_impl() override;

private:
    static void create_cb(doca_ec_task_create *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
    static void create_error_cb(doca_ec_task_create *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
    static void update_cb(doca_ec_task_update *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
    static void update_error_cb(doca_ec_task_update *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
    static void recover_cb(doca_ec_task_recover *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
    static void recover_error_cb(doca_ec_task_recover *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
};
}
