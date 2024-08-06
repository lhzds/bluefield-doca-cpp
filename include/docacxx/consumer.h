#pragma once

#include <doca_comch_consumer.h>
#include <doca_ctx.h>
#include <doca_pe.h>
#include <doca_mmap.h>
#include <doca_buf_inventory.h>
#include <functional>

#include "context.h"

namespace docacxx {
class Consumer: public ContextBase {
public:
    using CustomRecvCb = std::function<void()>;

    struct Cfg {
        size_t recv_task_num{};
    };
    struct Cap {
        uint32_t max_recv_task_num{};
        uint32_t max_buf_len{};
    };

    Consumer(doca_comch_connection *peer_conn, doca_pe *pe,
        doca_pe *conn_pe, doca_mmap *mmap, doca_dev *dev, const Cfg &cfg);
    ~Consumer() = default;

    bool is_connected() const { return state == DOCA_CTX_STATE_RUNNING; }

    Cap cap{};
    doca_comch_consumer *inner{};

    CustomRecvCb custom_recv_cb{[] {}};
    CustomRecvCb custom_recv_error_cb{[] {
        throw std::runtime_error("consumer recv error");
    }};

    doca_task *alloc_recv_task(doca_buf *buf);

protected:
    void clean_impl() override;

private:
    static void recv_cb(
        doca_comch_consumer_task_post_recv *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
    static void recv_error_cb(
        doca_comch_consumer_task_post_recv *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
};
}