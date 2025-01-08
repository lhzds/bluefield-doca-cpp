#pragma once

#include <doca_erasure_coding.h>
#include <functional>
#include <memory>
#include <stdexcept>

#include "docacxx/context.h"
#include "docacxx/mem.h"
#include "docacxx/task.hpp"

namespace docacxx {
class Ec: public Context {
public:
    using CreateTask = Task<doca_ec_task_create>;
    using UpdateTask = Task<doca_ec_task_update>;
    using RecoverTask = Task<doca_ec_task_recover>;

    struct Cfg {
        size_t create_task_slot_num{};
        size_t update_task_slot_num{};
        size_t recover_task_slot_num{};
    };

    Ec(doca_pe *pe, doca_dev *dev, const Cfg &cfg);
    ~Ec() {};

    doca_ec *raw() noexcept {
        return m_inner;
    }
    const doca_ec *raw() const noexcept {
        return m_inner;
    }

    doca_ec_matrix *create_matrix(doca_ec_matrix_type type,
        size_t data_block_count, size_t rdnc_block_count);
    doca_ec_matrix *create_recover_matrix(const struct doca_ec_matrix *coding_matrix,
        std::vector<uint32_t> &missing_indices);
    void destroy_matrix(doca_ec_matrix *);

    std::unique_ptr<CreateTask> alloc_create_task(const doca_ec_matrix *matrix,
        const Buffer &src, Buffer &dst, CreateTask::CompleteCallback cb);
    std::unique_ptr<CreateTask> submit(std::unique_ptr<CreateTask> task);
    void set_create_cb(CreateTask::ContextCompleteCallback cb) {
        m_create_cb = std::move(cb);
    }
    void set_create_error_cb(CreateTask::ContextCompleteCallback cb) {
        m_create_error_cb = std::move(cb);
    }

    std::unique_ptr<UpdateTask> alloc_update_task(const doca_ec_matrix *matrix,
        const Buffer &src, Buffer &dst, UpdateTask::CompleteCallback cb);
    std::unique_ptr<UpdateTask> submit(std::unique_ptr<UpdateTask> task);
    void set_update_cb(UpdateTask::ContextCompleteCallback cb) {
        m_update_cb = std::move(cb);
    }
    void set_update_error_cb(UpdateTask::ContextCompleteCallback cb) {
        m_update_error_cb = std::move(cb);
    }

    std::unique_ptr<RecoverTask> alloc_recover_task(const doca_ec_matrix *matrix,
        const Buffer &src, Buffer &dst, RecoverTask::CompleteCallback cb);
    std::unique_ptr<RecoverTask> submit(std::unique_ptr<RecoverTask> task);
    void set_recover_cb(RecoverTask::ContextCompleteCallback cb) {
        m_recover_cb = std::move(cb);
    }
    void set_recover_error_cb(RecoverTask::ContextCompleteCallback cb) {
        m_recover_error_cb = std::move(cb);
    }

protected:
    void clean_impl() override;

private:
    static void create_cb(doca_ec_task_create *raw_create_task,
        doca_data task_data,
        doca_data ctx_data);
    static void create_error_cb(doca_ec_task_create *raw_create_task,
        doca_data task_data,
        doca_data ctx_data);
    static void update_cb(doca_ec_task_update *raw_update_task,
        doca_data task_data,
        doca_data ctx_data);
    static void update_error_cb(doca_ec_task_update *raw_update_task,
        doca_data task_data,
        doca_data ctx_data);
    static void recover_cb(doca_ec_task_recover *raw_recover_task,
        doca_data task_data,
        doca_data ctx_data);
    static void recover_error_cb(doca_ec_task_recover *raw_recover_task,
        doca_data task_data,
        doca_data ctx_data);

    doca_ec *m_inner{};

    CreateTask::ContextCompleteCallback m_create_cb{[](std::unique_ptr<CreateTask>) {}};
    CreateTask::ContextCompleteCallback m_create_error_cb{[](std::unique_ptr<CreateTask>) {
        throw std::runtime_error("create error");
    }};
    UpdateTask::ContextCompleteCallback m_update_cb{[](std::unique_ptr<UpdateTask>) {}};
    UpdateTask::ContextCompleteCallback m_update_error_cb{[](std::unique_ptr<UpdateTask>) {
        throw std::runtime_error("update error");
    }};
    RecoverTask::ContextCompleteCallback m_recover_cb{[](std::unique_ptr<RecoverTask>) {}};
    RecoverTask::ContextCompleteCallback m_recover_error_cb{[](std::unique_ptr<RecoverTask>) {
        throw std::runtime_error("recover error");
    }};
};
}
