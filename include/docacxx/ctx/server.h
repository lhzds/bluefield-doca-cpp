#pragma once

#include <doca_comch_producer.h>
#include <doca_comch.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_pe.h>
#include <doca_types.h>
#include <fmt/core.h>
#include <functional>
#include <memory>

#include "../context.h"
#include "../task.hpp"

namespace docacxx {
namespace comch {
class Server: public Context {
public:
    using SendTask = Task<doca_comch_task_send>;
    using RecvCallback = std::function<void(doca_comch_connection *, uint8_t *, uint32_t)>;
    using ConnectionCallback = std::function<void(doca_comch_connection *)>;
    using ConsumerCallback = std::function<void(doca_comch_connection *, uint32_t)>;

    struct Cfg {
        size_t m_max_send_task_slot_num{1};
        // size_t max_msg_size{};
        // size_t max_recv_queue_size{};
    };

    Server(std::string_view name, doca_dev *dev, doca_dev_rep *dev_rep, doca_pe *pe, const Cfg &cfg);
    ~Server() {};

    doca_comch_server *raw() noexcept {
        return m_inner;
    }
    const doca_comch_server *raw() const noexcept {
        return m_inner;
    }

    std::unique_ptr<SendTask> alloc_send_task(doca_comch_connection *peer, const void *msg, uint32_t len,
        SendTask::CompleteCallback cb);
    std::unique_ptr<SendTask> submit(std::unique_ptr<SendTask> task);
    void set_send_cb(SendTask::ContextCompleteCallback cb) {
        m_send_cb = std::move(cb);
    }
    void set_send_error_cb(SendTask::ContextCompleteCallback cb) {
        m_send_error_cb = std::move(cb);
    }

    void set_recv_cb(RecvCallback cb) {
        m_recv_cb = std::move(cb);
    }
    void set_new_conn_cb(ConnectionCallback cb) {
        m_new_conn_cb = std::move(cb);
    }
    void set_disconn_cb(ConnectionCallback cb) {
        m_disconn_cb = std::move(cb);
    }

    void set_new_consumer_cb(ConsumerCallback cb) {
        m_new_consumer_cb = std::move(cb);
    }
    void set_expired_consumer_cb(ConsumerCallback cb) {
        m_expired_consumer_cb = std::move(cb);
    }

protected:
    void clean_impl() override;

private:
    static void send_cb(
        doca_comch_task_send *raw_send_task,
        doca_data task_data,
        doca_data ctx_data);
    static void send_error_cb(
        doca_comch_task_send *raw_send_task,
        doca_data task_data,
        doca_data ctx_data);
    static void recv_cb(
        doca_comch_event_msg_recv *event,
        uint8_t *msg_addr,
        uint32_t msg_len,
        struct doca_comch_connection *comch_connection);
    static void new_conn_cb(
        doca_comch_event_connection_status_changed *event,
        doca_comch_connection *comch_connection,
        uint8_t change_success);
    static void disconn_cb(
        doca_comch_event_connection_status_changed *event,
        doca_comch_connection *comch_connection,
        uint8_t change_success);
    static void new_consumer_cb(
        doca_comch_event_consumer *event,
        doca_comch_connection *comch_connection,
        uint32_t id);
    static void expired_consumer_cb(
        doca_comch_event_consumer *event,
        doca_comch_connection *comch_connection,
        uint32_t id);

    static Server *from_conn_to_server(doca_comch_connection *comch_connection);

    doca_comch_server *m_inner{};

    SendTask::ContextCompleteCallback m_send_cb{[](std::unique_ptr<SendTask>) {}};
    SendTask::ContextCompleteCallback m_send_error_cb{[](std::unique_ptr<SendTask>) {
        throw std::runtime_error("client send error");
    }};

    RecvCallback m_recv_cb{[](doca_comch_connection *, uint8_t *, uint32_t) {}};
    ConnectionCallback m_new_conn_cb{[](doca_comch_connection *) {}};
    ConnectionCallback m_disconn_cb{[](doca_comch_connection *) {
        throw std::runtime_error("peer is disconnected");
    }};
    ConsumerCallback m_new_consumer_cb{[](doca_comch_connection *, uint32_t) {}};
    ConsumerCallback m_expired_consumer_cb{ [](doca_comch_connection *, uint32_t id) {
        throw std::runtime_error(fmt::format("consumer with ID={} is expired", id));
    }};
};
}
}
