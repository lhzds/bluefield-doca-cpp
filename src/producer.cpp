#include "docacxx/producer.h"
#include "docacxx/doca_utils.h"
#include <doca_comch_producer.h>
#include <doca_mmap.h>
#include <doca_ctx.h>
#include <iostream>

Producer::Producer(
    doca_comch_connection *peer_conn, doca_dev *dev, const Cfg &cfg)
    : local_mem(dev, cfg.total_buf_size) {
    // 0. get cap
    doca_devinfo *dev_info = doca_dev_as_devinfo(dev);
    doca_comch_producer_cap_get_max_num_tasks(dev_info, &cap.max_tasks_num);
    doca_comch_producer_cap_get_max_buf_size(dev_info, &cap.max_buf_len);

    // 1. create context
    check_derr(
        doca_comch_producer_create(peer_conn, &inner),
        "failed to create comch producer");
    ctx = doca_comch_producer_as_ctx(inner);
    // 2. create pe
    check_derr(doca_pe_create(&pe), "failed to create pe");
    check_derr(doca_pe_connect_ctx(pe, ctx), "failed to connect to ctx");
    // 3. set recv task conf
    check_derr(
        doca_comch_producer_task_send_set_conf(
            inner, send_cb, send_error_cb, 1),
        "failed to register connection event");
    // 4. set context data
    doca_data ctx_user_data{};
    ctx_user_data.ptr = this;
    check_derr(
        doca_ctx_set_user_data(ctx, ctx_user_data),
        "failed to set user data of ctx");
    // 5. start context
    check_derr(doca_ctx_start(ctx), "failed to start ctx");
}

Producer::~Producer() {
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

    result = doca_comch_producer_destroy(inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy server properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
    result = doca_pe_destroy(pe);
    if (result != DOCA_SUCCESS)
        std::cerr << "Failed to destroy pe properly with error = "
                  << doca_error_get_name(result) << std::endl;
}

void Producer::send_cb(
    doca_comch_producer_task_send *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    auto *producer = (Producer *)ctx_user_data.ptr;
    producer->is_sended = true;
    producer->custom_send_cb();
}

void Producer::send_error_cb(
    doca_comch_producer_task_send *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    auto *producer = (Producer *)ctx_user_data.ptr;
    producer->custom_send_error_cb();
}

void Producer::send_block(std::string_view data, size_t remote_consumer_id) {
    is_sended = false;
    local_mem.set_data(data);

    doca_comch_producer_task_send *send_task{};
    check_derr(
        doca_comch_producer_task_send_alloc_init(
            inner, local_mem.buf, nullptr, 0, remote_consumer_id, &send_task),
        "failed to alloc send_task");

    doca_error result{};
    do {
        result =
            doca_task_submit(doca_comch_producer_task_send_as_task(send_task));
        if (result == DOCA_SUCCESS) { break; }
        if (result != DOCA_ERROR_AGAIN) {
            check_derr(result, "failed to summit send_task");
        }
        doca_pe_progress(pe);
    } while (true);

    while (!is_sended) { doca_pe_progress(pe); }
    doca_task_free(doca_comch_producer_task_send_as_task(send_task));
}