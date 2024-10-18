#pragma once

#include <doca_comch_producer.h>
#include <doca_comch.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_pe.h>
#include <doca_types.h>
#include <fmt/core.h>
#include <functional>

#include "context.h"

namespace docacxx {
namespace comch {
class Server: public ContextBase {
public:
    using CustomSendCb = std::function<void()>;
    using CustomRecvCb =
        std::function<void(uint8_t *, uint32_t, doca_comch_connection *)>;
    using CustomConnCb = std::function<void(doca_comch_connection *)>;
    using CustomConsumerCb =
        std::function<void(doca_comch_connection *, uint32_t)>;

    struct Cfg {
        size_t m_max_send_task_num{1};
        // size_t max_msg_size{};
        // size_t max_recv_queue_size{};
    };

    doca_comch_server *m_inner{};

    Server(std::string_view name, doca_dev *dev, doca_dev_rep *dev_rep, doca_pe *pe, const Cfg &cfg);

    CustomSendCb m_custom_send_cb{[] {}};
    CustomSendCb m_custom_send_error_cb{[] { throw std::runtime_error("client send error."); }};
    CustomRecvCb m_custom_recv_cb{[](uint8_t *, uint32_t, doca_comch_connection *) {}};
    CustomConnCb m_custom_new_conn_cb{[](doca_comch_connection *) {}};
    CustomConnCb m_custom_disconn_cb{[](doca_comch_connection *) {
        throw std::runtime_error("peer is disconnected");
    }};
    CustomConsumerCb m_custom_new_consumer_cb{[](doca_comch_connection *, uint32_t) {}};
    CustomConsumerCb m_custom_expired_consumer_cb{ [](doca_comch_connection *, uint32_t id) {
        throw std::runtime_error(fmt::format("consumer with ID=[{}] is expired.", id));
    }};

    doca_task *alloc_send_task(doca_comch_connection *peer, const void *msg, uint32_t len);

protected:
    void clean_impl() override;

private:
    static void send_cb(
        doca_comch_task_send *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
    static void send_error_cb(
        doca_comch_task_send *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
    static void recv_cb(
        doca_comch_event_msg_recv *,
        uint8_t *recv_buffer,
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
};
}
}
