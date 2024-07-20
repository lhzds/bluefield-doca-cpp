#pragma once

#include <doca_comch_producer.h>
#include <doca_comch.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_pe.h>
#include <doca_types.h>
#include <fmt/core.h>
#include <functional>

struct Client {
    using CustomSendCb = std::function<void()>;
    using CustomRecvCb =
        std::function<void(uint8_t *, uint32_t, doca_comch_connection *)>;
    using CustomConsumerCb =
        std::function<void(doca_comch_connection *, uint32_t)>;

    struct Cfg {
        // size_t max_msg_size{};
        // size_t max_recv_queue_size{};
    };

    bool is_connected{};
    doca_dev *dev{};
    doca_pe *pe{};
    doca_comch_client *client{};
    doca_ctx *ctx{};

    Client(
        std::string_view server_name,
        std::string_view dev_pci /* , const Cfg &cfg */);
    ~Client();
    doca_comch_connection *get_connection();

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

    void set_custom_send_cb(CustomSendCb cb) { custom_send_cb = cb; }
    void set_custom_send_error_cb(CustomSendCb cb) {
        custom_send_error_cb = cb;
    }
    void set_custom_recv_cb(CustomRecvCb cb) { custom_recv_cb = cb; }
    void set_custom_new_consumer_cb(CustomConsumerCb cb) {
        custom_new_consumer_cb = cb;
    }
    void set_custom_expired_consumer_cb(CustomConsumerCb cb) {
        custom_expired_consumer_cb = cb;
    }

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

  private:

  private:
    static Client *from_conn_to_client(doca_comch_connection *comch_connection);
};
