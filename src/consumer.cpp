#include "docacxx/consumer.h"
#include "docacxx/doca_utils.h"
#include <doca_buf_inventory.h>
#include <doca_buf.h>
#include <doca_error.h>
#include <doca_mmap.h>
#include <doca_ctx.h>
#include <doca_pe.h>
#include <iostream>

namespace docacxx {
Consumer::Consumer(
    doca_comch_connection *peer_conn, doca_pe *pe, doca_pe *conn_pe, doca_mmap *mmap,
    doca_dev *dev, const Cfg &cfg) {
    // get cap
    doca_devinfo *dev_info = doca_dev_as_devinfo(dev);
    doca_comch_consumer_cap_get_max_num_tasks(dev_info, &m_cap.m_max_recv_task_num);
    doca_comch_consumer_cap_get_max_buf_size(dev_info, &m_cap.m_max_buf_len);
    assert(cfg.m_recv_task_num <= m_cap.m_max_recv_task_num);

    // create context
    check_derr(
        doca_comch_consumer_create(peer_conn, mmap, &m_inner),
        "failed to create comch consumer");
    init_ctx(doca_comch_consumer_as_ctx(m_inner), pe);
    
    // create task pool
    check_derr(
        doca_comch_consumer_task_post_recv_set_conf(
            m_inner, recv_cb, recv_error_cb, cfg.m_recv_task_num),
        "failed to set conf of consuner recv task");
}

void Consumer::clean_impl() {
    auto result = doca_comch_consumer_destroy(m_inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy server properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
}

void Consumer::recv_cb(
    doca_comch_consumer_task_post_recv *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_comch_consumer_task_post_recv_as_task(task));
    auto *consumer = (Consumer *)ctx_user_data.ptr;
    consumer->custom_recv_cb();
}

void Consumer::recv_error_cb(
    doca_comch_consumer_task_post_recv *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_comch_consumer_task_post_recv_as_task(task));
    auto *consumer = (Consumer *)ctx_user_data.ptr;
    consumer->custom_recv_error_cb();
}

doca_task *Consumer::alloc_recv_task(doca_buf *buf) {
    doca_comch_consumer_task_post_recv *post_recv_task{};
    auto result = doca_comch_consumer_task_post_recv_alloc_init(
            m_inner, buf, &post_recv_task);
    check_derr(result, "failed to alloc post_recv_task");
    if (result == DOCA_ERROR_NO_MEMORY) {
        return nullptr;
    }
    return doca_comch_consumer_task_post_recv_as_task(post_recv_task);
}
}        
