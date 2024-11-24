#pragma once

#include <doca_comch_producer.h>
#include <doca_comch.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_pe.h>
#include <doca_types.h>
#include <fmt/core.h>
#include <functional>

#include "../context.h"
#include "docacxx/task.hpp"

namespace docacxx {
namespace comch {
class Client: public Context {
public:
    using SendTask = Task<doca_comch_task_send>;
    using RecvCallback = std::function<void(doca_comch_connection *, uint8_t *, uint32_t)>;
    using ConsumerCallback = std::function<void(doca_comch_connection *, uint32_t)>;

    struct Cfg {
        size_t m_max_send_task_slot_num{1};
        // size_t max_msg_size{};
        // size_t max_recv_queue_size{};
    };

    Client(std::string_view server_name, doca_dev *dev, doca_pe *pe, const Cfg &cfg);
    ~Client() {};

    doca_comch_client *raw() noexcept {
        return m_inner;
    }
    const doca_comch_client*raw() const noexcept {
        return m_inner;
    }

    doca_comch_connection *get_connection();
    const doca_comch_connection *get_connection() const;
    bool is_connected() const { return state() == DOCA_CTX_STATE_RUNNING; }

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
    static void new_consumer_cb(
        doca_comch_event_consumer *event,
        doca_comch_connection *comch_connection,
        uint32_t id);
    static void expired_consumer_cb(
        doca_comch_event_consumer *event,
        doca_comch_connection *comch_connection,
        uint32_t id);

    static Client *from_conn_to_client(doca_comch_connection *comch_connection);

    doca_comch_client *m_inner{};

    SendTask::ContextCompleteCallback m_send_cb{[](std::unique_ptr<SendTask>) {}};
    SendTask::ContextCompleteCallback m_send_error_cb{[](std::unique_ptr<SendTask>) {
        throw std::runtime_error("client send error");
    }};
    
    RecvCallback m_recv_cb{[](doca_comch_connection *, uint8_t *, uint32_t) {}};
    ConsumerCallback m_new_consumer_cb{[](doca_comch_connection *, uint32_t) {}};
    ConsumerCallback m_expired_consumer_cb{ [](doca_comch_connection *, uint32_t id) {
        throw std::runtime_error(fmt::format("consumer with ID={} is expired", id));
    }};
};
}
}
