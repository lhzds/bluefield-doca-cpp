#pragma once

#include "docacxx/context.h"
#include "docacxx/mem.h"
#include <doca_erasure_coding.h>
#include <functional>
#include <memory>
#include <stdexcept>

namespace docacxx {
class Ec: public Context {
public:
    struct Cfg {
        size_t m_create_task_slot_num{};
        size_t m_update_task_slot_num{};
        size_t m_recover_task_slot_num{};
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

    std::unique_ptr<Task> alloc_create_task(const doca_ec_matrix *matrix,
        const Buffer &src, Buffer &dst, Task::CompleteCallback cb);
    std::unique_ptr<Task> alloc_update_task(const doca_ec_matrix *matrix,
        const Buffer &src, Buffer &dst, Task::CompleteCallback cb);
    std::unique_ptr<Task> alloc_recover_task(const doca_ec_matrix *matrix,
        const Buffer &src, Buffer &dst, Task::CompleteCallback cb);

    void set_create_cb(ContextTaskCompleteCallback cb) {
        m_create_cb = std::move(cb);
    }
    void set_create_error_cb(ContextTaskCompleteCallback cb) {
        m_create_error_cb = std::move(cb);
    }
    void set_update_cb(ContextTaskCompleteCallback cb) {
        m_update_cb = std::move(cb);
    }
    void set_update_error_cb(ContextTaskCompleteCallback cb) {
        m_update_error_cb = std::move(cb);
    }
    void set_recover_cb(ContextTaskCompleteCallback cb) {
        m_recover_cb = std::move(cb);
    }
    void set_recover_error_cb(ContextTaskCompleteCallback cb) {
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

    ContextTaskCompleteCallback m_create_cb{[](std::unique_ptr<Task>) {}};
    ContextTaskCompleteCallback m_create_error_cb{[](std::unique_ptr<Task>) {
        throw std::runtime_error("create error");
    }};
    ContextTaskCompleteCallback m_update_cb{[](std::unique_ptr<Task>) {}};
    ContextTaskCompleteCallback m_update_error_cb{[](std::unique_ptr<Task>) {
        throw std::runtime_error("update error");
    }};
    ContextTaskCompleteCallback m_recover_cb{[](std::unique_ptr<Task>) {}};
    ContextTaskCompleteCallback m_recover_error_cb{[](std::unique_ptr<Task>) {
        throw std::runtime_error("recover error");
    }};
};
}
