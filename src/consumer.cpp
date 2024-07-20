#include "docacxx/consumer.h"
#include "docacxx/doca_utils.h"
#include <doca_buf_inventory.h>
#include <doca_buf.h>
#include <doca_error.h>
#include <doca_mmap.h>
#include <doca_ctx.h>
#include <doca_pe.h>
#include <iostream>

Consumer::Consumer(
    doca_comch_connection *peer_conn,
    doca_pe *conn_pe,
    doca_dev *dev,
    const Cfg &cfg)
    : local_mem(dev, cfg.total_buf_size) {
    // 0. get cap
    doca_devinfo *dev_info = doca_dev_as_devinfo(dev);
    doca_comch_consumer_cap_get_max_num_tasks(dev_info, &cap.max_tasks_num);
    doca_comch_consumer_cap_get_max_buf_size(dev_info, &cap.max_buf_len);

    // 1. create context
    check_derr(
        doca_comch_consumer_create(peer_conn, local_mem.mmap, &inner),
        "failed to create comch consumer");
    ctx = doca_comch_consumer_as_ctx(inner);
    // 2. create pe
    check_derr(doca_pe_create(&pe), "failed to create pe");
    check_derr(doca_pe_connect_ctx(pe, ctx), "failed to connect to ctx");
    // 3. set recv task conf
    check_derr(
        doca_comch_consumer_task_post_recv_set_conf(
            inner, recv_cb, recv_error_cb, 1),
        "failed to register connection event");
    // 4. set context data
    doca_data ctx_user_data{};
    ctx_user_data.ptr = this;
    check_derr(
        doca_ctx_set_user_data(ctx, ctx_user_data),
        "failed to set user data of ctx");
    // 5. start context
    check_derr(
        doca_ctx_start(ctx), "failed to start ctx", DOCA_ERROR_IN_PROGRESS);

    // 等待异步启动完成
    doca_ctx_states state{};
    do {
        doca_pe_progress(conn_pe);
        doca_pe_progress(pe);
        doca_ctx_get_state(ctx, &state);
    } while (state != DOCA_CTX_STATE_RUNNING);
}

Consumer::~Consumer() {
    doca_error result{};

    result = doca_ctx_stop(ctx);
    if (result != DOCA_SUCCESS) {
        if (result == DOCA_ERROR_IN_PROGRESS) {
            doca_ctx_states state{};
            do {
                doca_pe_progress(pe);
                doca_ctx_get_state(ctx, &state);
            } while (state != DOCA_CTX_STATE_IDLE);
        } else {
            std::cerr << "Failed to stop server ctx with error = "
                      << doca_error_get_name(result) << std::endl;
        }
    }

    result = doca_comch_consumer_destroy(inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy server properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
    result = doca_pe_destroy(pe);
    if (result != DOCA_SUCCESS)
        std::cerr << "Failed to destroy pe properly with error = "
                  << doca_error_get_name(result) << std::endl;
}

void Consumer::recv_cb(
    doca_comch_consumer_task_post_recv *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    auto *consumer = (Consumer *)ctx_user_data.ptr;
    consumer->is_recved = true;
    consumer->custom_recv_cb();
}

void Consumer::recv_error_cb(
    doca_comch_consumer_task_post_recv *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    auto *consumer = (Consumer *)ctx_user_data.ptr;
    consumer->custom_recv_error_cb();
}

std::string_view Consumer::recv_block() {
    is_recved = false;
    local_mem.clear_data();

    doca_comch_consumer_task_post_recv *post_recv_task{};
    check_derr(
        doca_comch_consumer_task_post_recv_alloc_init(
            inner, local_mem.buf, &post_recv_task),
        "failed to alloc post_recv_task");

    doca_error result{};
    do {
        result = doca_task_submit(
            doca_comch_consumer_task_post_recv_as_task(post_recv_task));
        if (result == DOCA_SUCCESS) { break; }
        if (result != DOCA_ERROR_AGAIN) {
            check_derr(result, "failed to submit post recv task");
        }
        doca_pe_progress(pe);
    } while (true);

    while (!is_recved) { doca_pe_progress(pe); }
    doca_task_free(doca_comch_consumer_task_post_recv_as_task(post_recv_task));
    return local_mem.get_data();
}
