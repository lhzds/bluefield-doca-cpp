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

std::unique_ptr<Producer::SendTask>
Producer::alloc_send_task(const Buffer &buf, uint32_t consumer_id, SendTask::CompleteCallback cb) {
    doca_comch_producer_task_send *task{};
    check_derr(doca_comch_producer_task_send_alloc_init(m_inner,
        buf.raw(), nullptr, 0, consumer_id, &task), "failed to alloc send_task");
    return std::make_unique<SendTask>(task, std::move(cb));
}

std::unique_ptr<Producer::SendTask>
Producer::alloc_send_task(uint8_t *imm_data, uint32_t imm_data_len, uint32_t consumer_id,
    SendTask::CompleteCallback cb) {
    doca_comch_producer_task_send *task{};
    check_derr(doca_comch_producer_task_send_alloc_init(m_inner,
        nullptr, imm_data, imm_data_len, consumer_id, &task), "failed to alloc send_task");
    return std::make_unique<SendTask>(task, std::move(cb));
}

std::unique_ptr<Producer::SendTask> Producer::submit(std::unique_ptr<Producer::SendTask> task) {
    auto *released = task.release();
    if (not submit_impl(released->raw_base())) {
        return std::unique_ptr<Producer::SendTask>(released);
    }
    return {};
}

void Producer::send_cb(doca_comch_producer_task_send *raw_send_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto task = std::unique_ptr<SendTask>((SendTask *)task_data.ptr);
    task->call_cb(); // 任务成功就会调用回调

    auto *producer = (Producer *)ctx_data.ptr;
    producer->m_send_cb(std::move(task));
}

void Producer::send_error_cb(doca_comch_producer_task_send *raw_send_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto task = std::unique_ptr<SendTask>((SendTask *)task_data.ptr);

    auto *producer = (Producer *)ctx_data.ptr;
    producer->m_send_cb(std::move(task));
}
}
}
