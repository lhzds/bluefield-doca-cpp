#pragma once

#include <doca_buf_inventory.h>
#include <doca_comch_producer.h>
#include <doca_pe.h>
#include <doca_mmap.h>
#include <functional>

#include "../context.h"
#include "../mem.h"
#include "docacxx/task.h"

namespace docacxx {
namespace comch {
class Producer: public Context {
public:
    using SendCallback = std::function<void()>;

    struct Cfg {
        size_t m_send_task_slot_num{};
    };
    struct Cap {
        uint32_t m_max_send_task_slot_num{};
        uint32_t m_max_buf_size{};
    };

    Producer(doca_comch_connection *peer_conn, doca_pe *pe,
        doca_dev *dev, const Cfg &cfg);
    ~Producer() {};

    doca_comch_producer *raw() noexcept {
        return m_inner;
    }
    const doca_comch_producer *raw() const noexcept {
        return m_inner;
    }
    const Cap &cap() const {
        return m_cap;
    }
    
    std::unique_ptr<Task> alloc_send_task(const Buffer &buf, uint8_t *imm_data,
        uint32_t imm_data_len, uint32_t consumer_id, Task::CompleteCallback cb);

    void set_send_cb(SendCallback cb) {
        m_send_cb = std::move(cb);
    }
    void set_recv_error_cb(SendCallback cb) {
        m_send_error_cb = std::move(cb);
    }

protected:
    void clean_impl() override;

private:
    static void send_cb(
        doca_comch_producer_task_send *raw_send_task,
        doca_data task_data,
        doca_data ctx_data);
    static void send_error_cb(
        doca_comch_producer_task_send *raw_send_task,
        doca_data task_data,
        doca_data ctx_data);

    Cap m_cap{};
    doca_comch_producer *m_inner{};

    SendCallback m_send_cb{[] {}};
    SendCallback m_send_error_cb{[] {
        throw std::runtime_error("producer send error");
    }};
};
}
}
