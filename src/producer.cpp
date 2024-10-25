#include "docacxx/ctx/producer.h"
#include "docacxx/doca_utils.h"
#include <doca_comch_producer.h>
#include <doca_mmap.h>
#include <doca_ctx.h>
#include <doca_pe.h>
#include <iostream>
#include <memory>

namespace docacxx {
namespace comch {
Producer::Producer(
    doca_comch_connection *peer_conn, doca_pe *pe, doca_dev *dev, const Cfg &cfg) {
    // get cap
    doca_devinfo *dev_info = doca_dev_as_devinfo(dev);
    doca_comch_producer_cap_get_max_num_tasks(dev_info, &m_cap.m_max_send_task_slot_num);
    doca_comch_producer_cap_get_max_buf_size(dev_info, &m_cap.m_max_buf_size);
    assert(cfg.m_send_task_slot_num <= m_cap.m_max_send_task_slot_num);

    // create context
    check_derr(doca_comch_producer_create(peer_conn, &m_inner),
        "failed to create comch producer");
    init_ctx(doca_comch_producer_as_ctx(m_inner), pe);
    check_derr(doca_comch_producer_task_send_set_conf(
        m_inner, send_cb, send_error_cb, cfg.m_send_task_slot_num),
        "failed to set conf of producer send task");
}

void Producer::clean_impl() {
    auto result = doca_comch_producer_destroy(m_inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy server properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
}

std::unique_ptr<Task> Producer::alloc_send_task(const Buffer &buf, uint8_t *imm_data,
    uint32_t imm_data_len, uint32_t consumer_id, Task::CompleteCallback cb) {
    doca_comch_producer_task_send *task{};
    check_derr(doca_comch_producer_task_send_alloc_init(m_inner,
        buf.raw(), imm_data, imm_data_len, consumer_id, &task), "failed to alloc send_task");
    return std::make_unique<Task>(doca_comch_producer_task_send_as_task(task), std::move(cb));
}

void Producer::send_cb(doca_comch_producer_task_send *raw_send_task,
    doca_data task_data,
    doca_data ctx_data) {
    std::unique_ptr<Task> task;
    task.reset((Task *)task_data.ptr);
    task->callback();
    task.reset();

    auto *producer = (Producer *)ctx_data.ptr;
    producer->m_send_cb();
}

void Producer::send_error_cb(doca_comch_producer_task_send *raw_send_task,
    doca_data task_data,
    doca_data ctx_data) {
    std::unique_ptr<Task> task;
    task.reset((Task *)task_data.ptr);
    task.reset();

    auto *producer = (Producer *)ctx_data.ptr;
    producer->m_send_cb();
}
}
}
