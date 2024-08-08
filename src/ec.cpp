#include "docacxx/ec.h"
#include <doca_erasure_coding.h>

namespace docacxx {
Ec::Ec(doca_pe *pe, doca_dev *dev, const Cfg &cfg) {
    check_derr(doca_ec_create(dev, &inner),
        "failed to create ec context");
    init_ctx(doca_ec_as_ctx(inner), pe);
    check_derr(doca_ec_task_create_set_conf(inner,
        create_cb, create_error_cb,
        cfg.create_task_num), "failed to create compress");
    check_derr(doca_ec_task_update_set_conf(inner,
        update_cb, update_error_cb,
        cfg.update_task_num), "failed to update compress");
    check_derr(doca_ec_task_recover_set_conf(inner,
        recover_cb, recover_error_cb,
        cfg.recover_task_num), "failed to recover compress");
}

void Ec::clean_impl() {
    auto result = doca_ec_destroy(inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy ec properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
}

doca_ec_matrix *Ec::create_matrix(doca_ec_matrix_type type,
    size_t data_block_count, size_t rdnc_block_count) {
    doca_ec_matrix *matrix{};
    check_derr(doca_ec_matrix_create(inner, type, data_block_count, rdnc_block_count, &matrix),
        "failed to create matrix");

    return matrix;
}

void Ec::destroy_matrix(doca_ec_matrix *matrix) {
    destroy_matrix(matrix);
}

doca_task *Ec::alloc_create_task(const doca_ec_matrix *matrix,
    const doca_buf *src, doca_buf *dst, doca_data task_user_data) {
    doca_ec_task_create *task{};
    auto result = doca_ec_task_create_allocate_init(inner, matrix, src, dst,
        task_user_data, &task);
    if (result == DOCA_ERROR_NO_MEMORY) {
        return nullptr;
    }
    check_derr(result, "failed to alloc ec create task");
    return doca_ec_task_create_as_task(task);
}

doca_task *Ec::alloc_update_task(const doca_ec_matrix *matrix,
    const doca_buf *src, doca_buf *dst, doca_data task_user_data) {
    doca_ec_task_update *task{};
    auto result = doca_ec_task_update_allocate_init(inner, matrix, src, dst,
        task_user_data, &task);
    if (result == DOCA_ERROR_NO_MEMORY) {
        return nullptr;
    }
    check_derr(result, "failed to alloc ec update task");
    return doca_ec_task_update_as_task(task);
}

doca_task *Ec::alloc_recover_task(const doca_ec_matrix *matrix,
    const doca_buf *src, doca_buf *dst, doca_data task_user_data) {
    doca_ec_task_recover *task{};
    auto result = doca_ec_task_recover_allocate_init(inner, matrix, src, dst,
        task_user_data, &task);
    if (result == DOCA_ERROR_NO_MEMORY) {
        return nullptr;
    }
    check_derr(result, "failed to alloc ec recover task");
    return doca_ec_task_recover_as_task(task);
}

void Ec::create_cb(doca_ec_task_create *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_ec_task_create_as_task(task));
    auto *compress = (Ec *)ctx_user_data.ptr;
    compress->custom_create_cb(task_user_data);
}

void Ec::create_error_cb(doca_ec_task_create *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_ec_task_create_as_task(task));
    auto *compress = (Ec *)ctx_user_data.ptr;
    compress->custom_create_error_cb(task_user_data);
}

void Ec::update_cb(doca_ec_task_update *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_ec_task_update_as_task(task));
    auto *compress = (Ec *)ctx_user_data.ptr;
    compress->custom_update_error_cb(task_user_data);
}

void Ec::update_error_cb(doca_ec_task_update *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_ec_task_update_as_task(task));
    auto *compress = (Ec *)ctx_user_data.ptr;
    compress->custom_update_error_cb(task_user_data);
}

void Ec::recover_cb(doca_ec_task_recover *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_ec_task_recover_as_task(task));
    auto *compress = (Ec *)ctx_user_data.ptr;
    compress->custom_recover_error_cb(task_user_data);
}

void Ec::recover_error_cb(doca_ec_task_recover *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_ec_task_recover_as_task(task));
    auto *compress = (Ec *)ctx_user_data.ptr;
    compress->custom_recover_error_cb(task_user_data);
}
}