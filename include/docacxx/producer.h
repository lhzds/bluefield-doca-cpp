#pragma once

#include <doca_buf_inventory.h>
#include <doca_comch_producer.h>
#include <doca_pe.h>
#include <doca_mmap.h>
#include <functional>

#include "doca_utils.h"

struct Producer {
    using CustomSendCb = std::function<void()>;

    struct Cfg {
        size_t total_buf_size{};
    };
    struct Cap {
        uint32_t max_tasks_num{};
        uint32_t max_buf_len{};
    };

    Producer(doca_comch_connection *peer_conn, doca_dev *dev, const Cfg &cfg);
    ~Producer();

    bool is_sended{};
    void send_block(std::string_view data, size_t remote_consumer_id);

    doca_pe *pe{};
    doca_ctx *ctx{};

    Cap cap{};
    doca_comch_producer *inner{};
    SingleLocalDocaMem local_mem;

    CustomSendCb custom_send_cb{[] {
    }};
    CustomSendCb custom_send_error_cb{[] {
        // throw std::runtime_error("producer send error");
    }};

    void set_custom_send_cb(CustomSendCb cb) { custom_send_cb = cb; }
    void set_custom_send_error_cb(CustomSendCb cb) {
        custom_send_error_cb = cb;
    }

    static void send_cb(
        doca_comch_producer_task_send *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
    static void send_error_cb(
        doca_comch_producer_task_send *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
};