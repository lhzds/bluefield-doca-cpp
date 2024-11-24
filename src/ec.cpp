#include "docacxx/ctx/ec.h"
#include <doca_erasure_coding.h>
#include <doca_types.h>
#include <iostream>

namespace docacxx {
Ec::Ec(doca_pe *pe, doca_dev *dev, const Cfg &cfg) {
    check_derr(doca_ec_create(dev, &m_inner),
        "failed to create ec context");
    init_ctx(doca_ec_as_ctx(m_inner), pe);
    check_derr(doca_ec_task_create_set_conf(m_inner,
        create_cb, create_error_cb,
        cfg.m_create_task_slot_num), "failed to set ec create task conf");
    check_derr(doca_ec_task_update_set_conf(m_inner,
        update_cb, update_error_cb,
        cfg.m_update_task_slot_num), "failed to set ec update task conf");
    check_derr(doca_ec_task_recover_set_conf(m_inner,
        recover_cb, recover_error_cb,
        cfg.m_recover_task_slot_num), "failed to set ec recover task conf");
}

void Ec::clean_impl() {
    auto result = doca_ec_destroy(m_inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy ec properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
}

doca_ec_matrix *Ec::create_matrix(doca_ec_matrix_type type,
    size_t data_block_count, size_t rdnc_block_count) {
    doca_ec_matrix *matrix{};
    check_derr(doca_ec_matrix_create(m_inner, type, data_block_count, rdnc_block_count, &matrix),
        "failed to create matrix");
    return matrix;
}

doca_ec_matrix *Ec::create_recover_matrix(const struct doca_ec_matrix *coding_matrix,
    std::vector<uint32_t> &missing_indices) {
    doca_ec_matrix *matrix{};
    doca_ec_matrix_create_recover(m_inner, coding_matrix,
        missing_indices.data(), missing_indices.size(), &matrix);
    return matrix;
}

void Ec::destroy_matrix(doca_ec_matrix *matrix) {
    check_derr(doca_ec_matrix_destroy(matrix), "failed to destroy matrix");
}

std::unique_ptr<Task> Ec::alloc_create_task(const doca_ec_matrix *matrix,
        const Buffer &src, Buffer &dst, Task::CompleteCallback cb) {
    doca_ec_task_create *task{};
    check_derr(doca_ec_task_create_allocate_init(m_inner, matrix, src.raw(),
        dst.raw(), doca_data{}, &task), "failed to alloc ec create task");
    return std::make_unique<Task>(doca_ec_task_create_as_task(task), std::move(cb));
}

std::unique_ptr<Task> Ec::alloc_update_task(const doca_ec_matrix *matrix,
        const Buffer &src, Buffer &dst, Task::CompleteCallback cb) {
    doca_ec_task_update *task{};
    check_derr(doca_ec_task_update_allocate_init(m_inner, matrix, src.raw(),
        dst.raw(), doca_data{}, &task), "failed to alloc ec update task");
    return std::make_unique<Task>(doca_ec_task_update_as_task(task), std::move(cb));
}

std::unique_ptr<Task> Ec::alloc_recover_task(const doca_ec_matrix *matrix,
        const Buffer &src, Buffer &dst, Task::CompleteCallback cb) {
    doca_ec_task_recover *task{};
    check_derr(doca_ec_task_recover_allocate_init(m_inner, matrix, src.raw(),
        dst.raw(), doca_data{}, &task), "failed to alloc ec recover task");
    return std::make_unique<Task>(doca_ec_task_recover_as_task(task), std::move(cb));
}

void Ec::create_cb(doca_ec_task_create *raw_create_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto *task = (Task *)task_data.ptr;
    task->call_cb(); // 任务成功就会调用回调
    delete task; // 析构任务

    auto *ec = (Ec *)ctx_data.ptr;
    ec->m_create_cb();
}

void Ec::create_error_cb(doca_ec_task_create *raw_create_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto *task = (Task *)task_data.ptr;
    delete task; // 析构任务

    auto *ec = (Ec *)ctx_data.ptr;
    ec->m_create_error_cb();
}

void Ec::update_cb(doca_ec_task_update *raw_update_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto *task = (Task *)task_data.ptr;
    task->call_cb(); // 任务成功就会调用回调
    delete task; // 析构任务

    auto *ec = (Ec *)ctx_data.ptr;
    ec->m_update_cb();
}

void Ec::update_error_cb(doca_ec_task_update *raw_update_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto *task = (Task *)task_data.ptr;
    delete task; // 析构任务

    auto *ec = (Ec *)ctx_data.ptr;
    ec->m_update_error_cb();
}

void Ec::recover_cb(doca_ec_task_recover *raw_recover_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto *task = (Task *)task_data.ptr;
    task->call_cb(); // 任务成功就会调用回调
    delete task; // 析构任务

    auto *ec = (Ec *)ctx_data.ptr;
    ec->m_recover_cb();
}

void Ec::recover_error_cb(doca_ec_task_recover *raw_recover_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto *task = (Task *)task_data.ptr;
    delete task; // 析构任务

    auto *ec = (Ec *)ctx_data.ptr;
    ec->m_recover_error_cb();
}
}
