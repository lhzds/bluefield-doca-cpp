#include "docacxx/doca_utils.h"
#include "docacxx/client.h"

#include <doca_ctx.h>
#include <iostream>

Client::Client(
    std::string_view server_name,
    std::string_view dev_pci /*, const Cfg &cfg_*/) {
    check_derr(
        open_doca_device_with_pci(dev_pci.data(), &dev), "failed to open dev");
    check_derr(doca_pe_create(&pe), "failed to create pe");
    check_derr(
        doca_comch_client_create(dev, server_name.data(), &client),
        "failed to create client");
    ctx = doca_comch_client_as_ctx(client);
    check_derr(doca_pe_connect_ctx(pe, ctx), "failed to connect to ctx");

    doca_comch_client_task_send_set_conf(client, send_cb, send_error_cb, 1024);
    doca_comch_client_event_msg_recv_register(client, recv_cb);
    check_derr(
        doca_comch_client_event_consumer_register(
            client, new_consumer_cb, expired_consumer_cb),
        "failed to register consumer event");

    // check_derr(doca_comch_client_set_max_msg_size(client, 4080), "failed to
    // set max msg size");
    // check_derr(doca_comch_client_set_recv_queue_size(client, 1024),
    //     "failed to set max queue size");

    doca_data ctx_user_data{};
    ctx_user_data.ptr = this;
    check_derr(
        doca_ctx_set_user_data(ctx, ctx_user_data),
        "failed to set user data of ctx");
    check_derr(
        doca_ctx_start(ctx), "failed to start ctx", DOCA_ERROR_IN_PROGRESS);

    doca_ctx_states state{};
    do {
        doca_pe_progress(pe);
        doca_ctx_get_state(ctx, &state);
    } while (state != DOCA_CTX_STATE_RUNNING);
}

Client::~Client() {
    doca_error result{};

    result = doca_ctx_stop(ctx);
    if (result != DOCA_SUCCESS) {
        if (result == DOCA_ERROR_IN_PROGRESS) {
            doca_ctx_states state{};
            do {
                doca_pe_progress(pe);
                doca_ctx_get_state(ctx, &state);
            } while (state != DOCA_CTX_STATE_IDLE);
        } else {
            std::cerr << "Failed to stop server ctx with error = "
                      << doca_error_get_name(result) << std::endl;
        }
    }

    doca_ctx_states state{};
    do {
        doca_pe_progress(pe);
        doca_ctx_get_state(ctx, &state);
    } while (state != DOCA_CTX_STATE_IDLE);

    result = doca_comch_client_destroy(client);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy client properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
    result = doca_pe_destroy(pe);
    if (result != DOCA_SUCCESS)
        std::cerr << "Failed to destroy pe properly with error = "
                  << doca_error_get_name(result) << std::endl;
    result = doca_dev_close(dev);
    if (result != DOCA_SUCCESS)
        std::cerr << "Failed to close dev properly with error = "
                  << doca_error_get_name(result) << std::endl;
}

doca_comch_connection *Client::get_connection() {
    doca_comch_connection *conn{};
    check_derr(
        doca_comch_client_get_connection(client, &conn),
        "failed to get connection from client");
    return conn;
}

Client *Client::from_conn_to_client(doca_comch_connection *comch_connection) {
    auto *client_ctx = doca_comch_client_get_client_ctx(comch_connection);
    doca_data ctx_user_data{};
    check_derr(
        doca_ctx_get_user_data(
            doca_comch_client_as_ctx(client_ctx), &ctx_user_data),
        "failed to get user data of ctx");
    return (Client *)ctx_user_data.ptr;
}

void Client::new_consumer_cb(
    doca_comch_event_consumer *event,
    doca_comch_connection *comch_connection,
    uint32_t id) {
    auto *client = from_conn_to_client(comch_connection);
    client->custom_new_consumer_cb(comch_connection, id);
}

void Client::expired_consumer_cb(
    doca_comch_event_consumer *event,
    doca_comch_connection *comch_connection,
    uint32_t id) {
    auto *client = from_conn_to_client(comch_connection);
    client->custom_expired_consumer_cb(comch_connection, id);
}

void Client::send_cb(
    doca_comch_task_send *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    auto *client = (Client *)ctx_user_data.ptr;
    client->custom_send_cb();
}

void Client::send_error_cb(
    doca_comch_task_send *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    auto *client = (Client *)ctx_user_data.ptr;
    client->custom_send_error_cb();
}

void Client::recv_cb(
    doca_comch_event_msg_recv *,
    uint8_t *recv_buffer,
    uint32_t msg_len,
    struct doca_comch_connection *comch_connection) {
    auto *client = from_conn_to_client(comch_connection);
    client->custom_recv_cb(recv_buffer, msg_len, comch_connection);
}