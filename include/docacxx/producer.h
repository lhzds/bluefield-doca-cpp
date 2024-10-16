#pragma once

#include <doca_buf_inventory.h>
#include <doca_comch_producer.h>
#include <doca_pe.h>
#include <doca_mmap.h>
#include <functional>

#include "context.h"

namespace docacxx {
namespace comch {
class Producer: public ContextBase {
public:
    using CustomSendCb = std::function<void()>;

    struct Cfg {
        size_t m_send_task_num{};
    };
    struct Cap {
        uint32_t m_max_send_task_num{};
        uint32_t m_max_buf_len{};
    };

    Producer(doca_comch_connection *peer_conn, doca_pe *pe,
        doca_dev *dev, const Cfg &cfg);
    ~Producer() = default;

    Cap m_cap{};
    doca_comch_producer *m_inner{};

    CustomSendCb m_custom_send_cb{[] {}};
    CustomSendCb m_custom_send_error_cb{[] {
        throw std::runtime_error("producer send error");
    }};
    
    doca_task *alloc_send_task(const doca_buf *buf, uint8_t *imm_data,
		uint32_t imm_data_len, uint32_t consumer_id);

protected:
    void clean_impl() override;

private:
    static void send_cb(
        doca_comch_producer_task_send *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
    static void send_error_cb(
        doca_comch_producer_task_send *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
};
}
}
