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

namespace docacxx {
namespace comch {
class Consumer: public Context {
public:
    using RecvCallback = std::function<void()>;

    struct Cfg {
        size_t m_recv_task_slot_num{};
    };
    struct Cap {
        uint32_t m_max_recv_task_slot_num{};
        uint32_t m_max_buf_size{};
    };

    Consumer(doca_comch_connection *peer_conn, doca_pe *pe,
        doca_pe *conn_pe, Mmap &mmap, doca_dev *dev, const Cfg &cfg);
    virtual ~Consumer() = default;

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
    std::unique_ptr<Task> alloc_recv_task(Buffer &buf, Task::CompleteCallback cb);

    void set_recv_cb(RecvCallback cb) {
        m_recv_cb = std::move(cb);
    }
    void set_recv_error_cb(RecvCallback cb) {
        m_recv_error_cb = std::move(cb);
    }

protected:
    void clean_impl() override;

private:
    static void recv_cb(
        doca_comch_consumer_task_post_recv *raw_post_recv_task,
        doca_data task_data,
        doca_data ctx_data);
    static void recv_error_cb(
        doca_comch_consumer_task_post_recv *raw_post_recv_task,
        doca_data task_data,
        doca_data ctx_data);

    Cap m_cap{};
    doca_comch_consumer *m_inner{};

    RecvCallback m_recv_cb{[]() {}};
    RecvCallback m_recv_error_cb{[]() {
        throw std::runtime_error("consumer recv error");
    }};
};
}
}
