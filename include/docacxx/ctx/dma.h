#pragma once

#include "docacxx/context.h"
#include "docacxx/mem.h"
#include <doca_dma.h>
#include <memory>
#include <stdexcept>

namespace docacxx {
class Dma: public Context {
public:
    struct Cfg {
        size_t m_memcpy_task_slot_num{};
    };
    struct Cap {
        size_t m_max_buf_size;
    };

    Dma(doca_pe *pe, doca_dev *dev, const Cfg &cfg);
    ~Dma() {};

    const Cap &cap() const noexcept {
        return m_cap;
    }
    doca_dma *raw() noexcept {
        return m_inner;
    }
    const doca_dma *raw() const noexcept {
        return m_inner;
    }

    std::unique_ptr<Task> alloc_memcpy_task(const Buffer &src, Buffer &dst,
        Task::CompleteCallback cb);
        
    void set_memcpy_cb(ContextTaskCompleteCallback cb) {
        m_memcpy_cb = std::move(cb);
    }
    void set_memcpy_error_cb(ContextTaskCompleteCallback cb) {
        m_memcpy_error_cb = std::move(cb);
    }

protected:
    void clean_impl() override;

private:
    static void memcpy_cb(doca_dma_task_memcpy *raw_memcpy_task,
        doca_data task_data,
        doca_data ctx_data);
    static void memcpy_error_cb(doca_dma_task_memcpy *task,
		doca_data task_user_data,
		doca_data ctx_user_data);

    doca_dma *m_inner{};
    Cap m_cap{};

    ContextTaskCompleteCallback m_memcpy_cb{[](std::unique_ptr<Task>) {}};
    ContextTaskCompleteCallback m_memcpy_error_cb{[](std::unique_ptr<Task>) {
        throw std::runtime_error("memcpy error");
    }};
};
}
