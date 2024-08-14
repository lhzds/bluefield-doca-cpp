#include "docacxx/doca_utils.h"
#include "docacxx/client.h"

#include <doca_ctx.h>
#include <iostream>

namespace docacxx {
Client::Client(
    std::string_view server_name,
    doca_dev *dev, doca_pe *pe, const Cfg &cfg) {
    check_derr(
        doca_comch_client_create(dev, server_name.data(), &client),
        "failed to create client");    
    init_ctx(doca_comch_client_as_ctx(client), pe);

    check_derr(doca_comch_client_event_consumer_register(
            client, new_consumer_cb, expired_consumer_cb),
        "failed to register consumer event");

    check_derr(doca_comch_client_task_send_set_conf(client, send_cb, send_error_cb, cfg.max_send_task_num),
        "failed to set send task conf");
    check_derr(doca_comch_client_event_msg_recv_register(client, recv_cb),
        "failed to register recv msg event");

    // check_derr(doca_comch_client_set_max_msg_size(client, 4080), "failed to
    // set max msg size");
    // check_derr(doca_comch_client_set_recv_queue_size(client, 1024),
    //     "failed to set max queue size");
}

void Client::clean_impl() {
    auto result = doca_comch_client_destroy(client);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy client properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
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
    doca_task_free(doca_comch_task_send_as_task(task));
    auto *client = (Client *)ctx_user_data.ptr;
    client->custom_send_cb();
}

void Client::send_error_cb(
    doca_comch_task_send *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_comch_task_send_as_task(task));
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
}
