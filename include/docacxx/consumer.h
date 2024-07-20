#pragma once

#include <doca_comch_consumer.h>
#include <doca_ctx.h>
#include <doca_pe.h>
#include <doca_mmap.h>
#include <doca_buf_inventory.h>
#include <functional>

#include "doca_utils.h"
#include "client.h"

struct Consumer {
    using CustomRecvCb = std::function<void()>;

    struct Cfg {
        size_t total_buf_size{};
    };
    struct Cap {
        uint32_t max_tasks_num{};
        uint32_t max_buf_len{};
    };

    Consumer(
        doca_comch_connection *peer_conn,
        doca_pe *conn_pe,
        doca_dev *dev,
        const Cfg &cfg);
    ~Consumer();

    bool is_recved{};
    std::string_view recv_block();

    doca_pe *pe{};
    doca_ctx *ctx{};

    Cap cap{};
    doca_comch_consumer *inner{};
    SingleLocalDocaMem local_mem;

    CustomRecvCb custom_recv_cb{[] {
    }};
    CustomRecvCb custom_recv_error_cb{[] {
        // throw std::runtime_error("consumer recv error");
    }};

    void set_custom_recv_cb(CustomRecvCb cb) { custom_recv_cb = cb; }
    void set_custom_recv_error_cb(CustomRecvCb cb) {
        custom_recv_error_cb = cb;
    }

    static void recv_cb(
        doca_comch_consumer_task_post_recv *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
    static void recv_error_cb(
        doca_comch_consumer_task_post_recv *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
};