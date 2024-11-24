#include "docacxx/ctx/dma.h"
#include <iostream>
#include <memory>

namespace docacxx {
Dma::Dma(doca_pe *pe, doca_dev *dev, const Cfg &cfg) {
    // get cap
    doca_devinfo *dev_info = doca_dev_as_devinfo(dev);
    doca_dma_cap_task_memcpy_get_max_buf_size(dev_info, &m_cap.m_max_buf_size);

    check_derr(doca_dma_create(dev, &m_inner),
        "failed to create dma context");
    init_ctx(doca_dma_as_ctx(m_inner), pe);
    check_derr(doca_dma_task_memcpy_set_conf(m_inner, memcpy_cb, memcpy_error_cb,
        cfg.m_memcpy_task_slot_num), "failed to set dma memcpy task conf");
}

void Dma::clean_impl() {
    auto result = doca_dma_destroy(m_inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy dma properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
}

std::unique_ptr<Task> Dma::alloc_memcpy_task(const Buffer &src, Buffer &dst, Task::CompleteCallback cb) {
    doca_dma_task_memcpy *task{};
    check_derr(doca_dma_task_memcpy_alloc_init(m_inner, src.raw(),
        dst.raw(), doca_data{}, &task), "failed to alloc dma memcpy task");
    return std::make_unique<Task>(doca_dma_task_memcpy_as_task(task), std::move(cb));
}

void Dma::memcpy_cb(doca_dma_task_memcpy *raw_memcpy_task,
    doca_data task_data, doca_data ctx_data) {
    auto task = std::unique_ptr<Task>((Task *)task_data.ptr);
    task->call_cb(); // 任务成功就会调用回调

    auto *dma = (Dma *)ctx_data.ptr;
    dma->m_memcpy_cb(std::move(task));
}

void Dma::memcpy_error_cb(doca_dma_task_memcpy *raw_memcpy_task,
    doca_data task_data, doca_data ctx_data) {
    auto task = std::unique_ptr<Task>((Task *)task_data.ptr);

    auto *dma = (Dma *)ctx_data.ptr;
    dma->m_memcpy_error_cb(std::move(task));
}
}
