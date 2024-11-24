#pragma once

#include <doca_comch_consumer.h>
#include <doca_ctx.h>
#include <doca_pe.h>
#include <doca_mmap.h>
#include <doca_buf_inventory.h>
#include <functional>
#include <memory>

#include "../context.h"
#include "../mem.h"
#include "../task.hpp"

namespace docacxx {
namespace comch {
class Consumer: public Context {
public:
    using PostRecvTask = Task<doca_comch_consumer_task_post_recv>;

    struct Cfg {
        size_t m_recv_task_slot_num{};
    };
    struct Cap {
        uint32_t m_max_recv_task_slot_num{};
        uint32_t m_max_buf_size{};
    };

    Consumer(doca_comch_connection *peer_conn, doca_pe *pe,
        doca_pe *conn_pe, Mmap &mmap, doca_dev *dev, const Cfg &cfg);
    ~Consumer() {};

    doca_comch_consumer *raw() noexcept {
        return m_inner;
    }
    const doca_comch_consumer *raw() const noexcept {
        return m_inner;
    }
    const Cap &cap() const {
        return m_cap;
    }

    bool is_connected() const { return state() == DOCA_CTX_STATE_RUNNING; }
    std::unique_ptr<PostRecvTask> alloc_post_recv_task(Buffer &buf, PostRecvTask::CompleteCallback cb);
    std::unique_ptr<PostRecvTask> submit(std::unique_ptr<PostRecvTask> task);
    void set_post_recv_cb(PostRecvTask::ContextCompleteCallback cb) {
        m_post_recv_cb = std::move(cb);
    }
    void set_post_recv_error_cb(PostRecvTask::ContextCompleteCallback cb) {
        m_post_recv_error_cb = std::move(cb);
    }

protected:
    void clean_impl() override;

private:
    static void post_recv_cb(
        doca_comch_consumer_task_post_recv *raw_post_recv_task,
        doca_data task_data,
        doca_data ctx_data);
    static void post_recv_error_cb(
        doca_comch_consumer_task_post_recv *raw_post_recv_task,
        doca_data task_data,
        doca_data ctx_data);

    Cap m_cap{};
    doca_comch_consumer *m_inner{};

    PostRecvTask::ContextCompleteCallback m_post_recv_cb{[](std::unique_ptr<PostRecvTask>) {}};
    PostRecvTask::ContextCompleteCallback m_post_recv_error_cb{[](std::unique_ptr<PostRecvTask>) {
        throw std::runtime_error("consumer recv error");
    }};
};
}
}
