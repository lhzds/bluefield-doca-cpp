#pragma once

#include "docacxx/context.h"
#include <doca_dma.h>

namespace docacxx {
class Dma: public ContextBase {
public:
    using CustomMemcpyCb = std::function<void(doca_data)>;

    struct Cfg {
        size_t m_memcpy_task_num{};
    };

    Dma(doca_pe *pe, doca_dev *dev, const Cfg &cfg);
    virtual ~Dma() = default;

    doca_dma *m_inner{};

    CustomMemcpyCb m_custom_memcpy_cb{[](doca_data) {}};
    CustomMemcpyCb m_custom_memcpy_error_cb{[](doca_data) {}};

    doca_task *alloc_memcpy_task(const doca_buf *src, doca_buf *dst, doca_data task_user_data = doca_data{});

protected:
    void clean_impl() override;

private:
    static void memcpy_cb(doca_dma_task_memcpy *task,
		doca_data task_user_data,
		doca_data ctx_user_data);
    static void memcpy_error_cb(doca_dma_task_memcpy *task,
		doca_data task_user_data,
		doca_data ctx_user_data);
};
}
