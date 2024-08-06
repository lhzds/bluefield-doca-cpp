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
class Client: public ContextBase {
    using CustomSendCb = std::function<void()>;
    using CustomRecvCb =
        std::function<void(uint8_t *, uint32_t, doca_comch_connection *)>;
    using CustomConsumerCb =
        std::function<void(doca_comch_connection *, uint32_t)>;

    struct Cfg {
        size_t max_send_task_num{};
        // size_t max_msg_size{};
        // size_t max_recv_queue_size{};
    };

    doca_dev *dev{};
    doca_comch_client *client{};

    Client(std::string_view server_name,
        std::string_view dev_pci, doca_pe *pe , const Cfg &cfg);
    
    void clean_impl() override;

    doca_comch_connection *get_connection();
    bool is_connected() const { return state == DOCA_CTX_STATE_RUNNING; }

    CustomSendCb custom_send_cb{[] {
    }};
    CustomSendCb custom_send_error_cb{[] {
        throw std::runtime_error("client send error.");
    }};
    CustomRecvCb custom_recv_cb{
        [](uint8_t *, uint32_t, doca_comch_connection *) {
        }};
    CustomConsumerCb custom_new_consumer_cb{
        [](doca_comch_connection *, uint32_t) {
        }};
    CustomConsumerCb custom_expired_consumer_cb{
        [](doca_comch_connection *, uint32_t id) {
            throw std::runtime_error(
                fmt::format("consumer with ID=[{}] is expired.", id));
        }};

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
    static void new_consumer_cb(
        doca_comch_event_consumer *event,
        doca_comch_connection *comch_connection,
        uint32_t id);
    static void expired_consumer_cb(
        doca_comch_event_consumer *event,
        doca_comch_connection *comch_connection,
        uint32_t id);

    static Client *from_conn_to_client(doca_comch_connection *comch_connection);
};
}