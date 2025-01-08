#include "docacxx/ctx/consumer.h"
#include "docacxx/doca_utils.h"
#include <doca_buf_inventory.h>
#include <doca_buf.h>
#include <doca_error.h>
#include <doca_mmap.h>
#include <doca_ctx.h>
#include <doca_pe.h>
#include <iostream>

namespace docacxx {
namespace comch {
Consumer::Consumer(
    doca_comch_connection *peer_conn, doca_pe *pe, doca_pe *conn_pe, Mmap &mmap,
    doca_dev *dev, const Cfg &cfg) {
    // get cap
    doca_devinfo *dev_info = doca_dev_as_devinfo(dev);
    doca_comch_consumer_cap_get_max_num_tasks(dev_info, &m_cap.max_post_recv_task_slot_num);
    doca_comch_consumer_cap_get_max_buf_size(dev_info, &m_cap.max_buf_size);
    assert(cfg.m_post_recv_task_slot_num <= m_cap.m_max_post_recv_task_slot_num);

    // create context
    check_derr(doca_comch_consumer_create(peer_conn, mmap.raw(), &m_inner),
        "failed to create comch consumer");
    init_ctx(doca_comch_consumer_as_ctx(m_inner), pe, conn_pe);
    
    // set task conf
    check_derr(
        doca_comch_consumer_task_post_recv_set_conf(
            m_inner, post_recv_cb, post_recv_error_cb, cfg.post_recv_task_slot_num),
        "failed to set conf of consuner recv task");
}

void Consumer::clean_impl() {
    auto result = doca_comch_consumer_destroy(m_inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy server properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
}

std::unique_ptr<Consumer::PostRecvTask> Consumer::alloc_post_recv_task(Buffer &buf,
    PostRecvTask::CompleteCallback cb) {
    doca_comch_consumer_task_post_recv *task{};
    check_derr(doca_comch_consumer_task_post_recv_alloc_init(
            m_inner, buf.raw(), &task), "failed to alloc post_recv_task");
    return std::make_unique<PostRecvTask>(task, &buf, std::move(cb));
}

// 用于接收立即数
std::unique_ptr<Consumer::PostRecvTask> Consumer::alloc_post_recv_task(
    PostRecvTask::CompleteCallback cb) {
    doca_comch_consumer_task_post_recv *task{};
    check_derr(doca_comch_consumer_task_post_recv_alloc_init(
            m_inner, nullptr, &task), "failed to alloc post_recv_task");
    return std::make_unique<PostRecvTask>(task, nullptr, std::move(cb));
}

std::unique_ptr<Consumer::PostRecvTask> Consumer::submit(std::unique_ptr<Consumer::PostRecvTask> task) {
    auto *released = task.release();
    if (not submit_impl(released->raw_base())) {
        return std::unique_ptr<Consumer::PostRecvTask>(released);
    }
    return {};
}

void Consumer::post_recv_cb(
    doca_comch_consumer_task_post_recv *raw_post_recv_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto task = std::unique_ptr<Consumer::PostRecvTask>((Consumer::PostRecvTask *)task_data.ptr);
    task->call_cb(); // 任务成功就会调用回调

    auto *consumer = (Consumer *)ctx_data.ptr;
    consumer->m_post_recv_cb(std::move(task));
}

void Consumer::post_recv_error_cb(
    doca_comch_consumer_task_post_recv *raw_post_recv_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto task = std::unique_ptr<Consumer::PostRecvTask>((Consumer::PostRecvTask *)task_data.ptr);

    auto *consumer = (Consumer *)ctx_data.ptr;
    consumer->m_post_recv_error_cb(std::move(task));
}
}        
}
