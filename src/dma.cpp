#include "docacxx/dma.h"
#include <doca_dma.h>
#include <doca_error.h>

namespace docacxx {
Dma::Dma(doca_pe *pe, doca_dev *dev, const Cfg &cfg) {
    check_derr(doca_dma_create(dev, &m_inner),
        "failed to create dma context");
    init_ctx(doca_dma_as_ctx(m_inner), pe);
    check_derr(doca_dma_task_memcpy_set_conf(m_inner,
        memcpy_cb, memcpy_error_cb,
        cfg.m_memcpy_task_num), "failed to create compress");
}

void Dma::clean_impl() {
    auto result = doca_dma_destroy(m_inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy dma properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
}

doca_task *Dma::alloc_memcpy_task(const doca_buf *src, doca_buf *dst, doca_data task_user_data) {
    doca_dma_task_memcpy *task{};
    auto result = doca_dma_task_memcpy_alloc_init(m_inner, src, dst,
        task_user_data, &task);
    if (result == DOCA_ERROR_NO_MEMORY) {
        return nullptr;
    }
    check_derr(result, "failed to alloc dma memcpy task");
    return doca_dma_task_memcpy_as_task(task);
}

void Dma::memcpy_cb(doca_dma_task_memcpy *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_dma_task_memcpy_as_task(task));
    auto *dma = (Dma *)ctx_user_data.ptr;
    dma->m_custom_memcpy_cb();
}

void Dma::memcpy_error_cb(doca_dma_task_memcpy *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_dma_task_memcpy_as_task(task));
    auto *dma = (Dma *)ctx_user_data.ptr;
    dma->m_custom_memcpy_error_cb();
}
}
