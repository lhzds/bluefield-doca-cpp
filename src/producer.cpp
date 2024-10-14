#include "docacxx/producer.h"
#include "docacxx/doca_utils.h"
#include <doca_comch_producer.h>
#include <doca_mmap.h>
#include <doca_ctx.h>
#include <doca_pe.h>
#include <iostream>

namespace docacxx {
Producer::Producer(
    doca_comch_connection *peer_conn, doca_pe *pe_, doca_dev *dev, const Cfg &cfg) {
    // get cap
    doca_devinfo *dev_info = doca_dev_as_devinfo(dev);
    doca_comch_producer_cap_get_max_num_tasks(dev_info, &m_cap.m_max_send_task_num);
    doca_comch_producer_cap_get_max_buf_size(dev_info, &m_cap.m_max_buf_len);
    assert(cfg.send_task_num <= cap.max_send_task_num);

    // create context
    check_derr(
        doca_comch_producer_create(peer_conn, &m_inner),
        "failed to create comch producer");
    init_ctx(doca_comch_producer_as_ctx(m_inner), pe_);

    // create task pool
    check_derr(
        doca_comch_producer_task_send_set_conf(
            m_inner, send_cb, send_error_cb, cfg.m_send_task_num),
        "failed to set conf of producer send task");
}

void Producer::clean_impl() {
    auto result = doca_comch_producer_destroy(m_inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy server properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
}

doca_task *Producer::alloc_send_task(const doca_buf *buf, uint8_t *imm_data,
    uint32_t imm_data_len, uint32_t consumer_id) {
    doca_comch_producer_task_send *send_task{};
    auto result = doca_comch_producer_task_send_alloc_init(m_inner,
        buf, imm_data, imm_data_len, consumer_id, &send_task);
    if (result == DOCA_ERROR_NO_MEMORY) {
        return nullptr;
    }
    check_derr(result, "failed to alloc send_task");
    return doca_comch_producer_task_send_as_task(send_task);
}

void Producer::send_cb(
    doca_comch_producer_task_send *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_comch_producer_task_send_as_task(task));
    auto *producer = (Producer *)ctx_user_data.ptr;
    producer->m_custom_send_cb();
}

void Producer::send_error_cb(
    doca_comch_producer_task_send *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_comch_producer_task_send_as_task(task));
    auto *producer = (Producer *)ctx_user_data.ptr;
    producer->m_custom_send_error_cb();
}
}
