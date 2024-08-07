#include "docacxx/doca_utils.h"
#include "docacxx/server.h"

#include <doca_comch_producer.h>
#include <doca_error.h>
#include <iostream>

namespace docacxx {
Server::Server(
    std::string_view name,
    std::string_view dev_pci,
    std::string_view dev_rep_pci,
    doca_pe *pe, const Cfg &cfg) {
    check_derr(
        open_doca_device_with_pci(dev_pci.data(), &dev), "failed to open dev");
    check_derr(
        open_doca_device_rep_with_pci(
            dev, DOCA_DEVINFO_REP_FILTER_NET, dev_rep_pci.data(), &dev_rep),
        "failed to open rep");

    check_derr(
        doca_comch_server_create(dev, dev_rep, name.data(), &inner),
        "failed to create server");
    init_ctx(doca_comch_server_as_ctx(inner), pe);

    check_derr(
        doca_comch_server_event_connection_register(
            inner, new_conn_cb, disconn_cb),
        "failed to register connection event");
    check_derr(
        doca_comch_server_event_consumer_register(
            inner, new_consumer_cb, expired_consumer_cb),
        "failed to register consumer event");

    check_derr(doca_comch_server_task_send_set_conf(inner, send_cb, send_error_cb, cfg.max_send_task_num),
        "failed to set send task conf");
    check_derr(doca_comch_server_event_msg_recv_register(inner, recv_cb),
        "failed to register recv msg event");

    // check_derr(doca_comch_server_set_max_msg_size(inner, cfg.max_msg_size),
    // "failed to set max msg size");
    // check_derr(doca_comch_server_set_recv_queue_size(inner,
    // cfg.max_recv_queue_size),
    //     "failed to set max queue size");
}

void Server::clean_impl() {
    auto result = doca_comch_server_destroy(inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy server properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
    result = doca_dev_close(dev);
    if (result != DOCA_SUCCESS)
        std::cerr << "Failed to close dev properly with error = "
                  << doca_error_get_name(result) << std::endl;
    result = doca_dev_rep_close(dev_rep);
    if (result != DOCA_SUCCESS)
        std::cerr << "Failed to close dev rep properly with error = "
                  << doca_error_get_name(result) << std::endl;
}

Server *Server::from_conn_to_server(doca_comch_connection *comch_connection) {
    auto *server_ctx = doca_comch_server_get_server_ctx(comch_connection);
    doca_data ctx_user_data{};
    check_derr(
        doca_ctx_get_user_data(
            doca_comch_server_as_ctx(server_ctx), &ctx_user_data),
        "failed to get user data of ctx");
    return (Server *)ctx_user_data.ptr;
}

void Server::send_cb(
    doca_comch_task_send *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_comch_task_send_as_task(task));
    auto *server = (Server *)ctx_user_data.ptr;
    server->custom_send_error_cb();
}

void Server::send_error_cb(
    doca_comch_task_send *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_comch_task_send_as_task(task));
    auto *server = (Server *)ctx_user_data.ptr;
    server->custom_send_error_cb();
}

void Server::recv_cb(
    doca_comch_event_msg_recv *,
    uint8_t *recv_buffer,
    uint32_t msg_len,
    struct doca_comch_connection *comch_connection) {
    auto *server = from_conn_to_server(comch_connection);
    server->custom_recv_cb(recv_buffer, msg_len, comch_connection);
}

void Server::new_conn_cb(
    doca_comch_event_connection_status_changed *event,
    doca_comch_connection *comch_connection,
    uint8_t change_success) {
    auto *server = from_conn_to_server(comch_connection);
    server->custom_new_conn_cb(comch_connection);
}

void Server::disconn_cb(
    doca_comch_event_connection_status_changed *event,
    doca_comch_connection *comch_connection,
    uint8_t change_success) {
    auto *server = from_conn_to_server(comch_connection);
    server->custom_disconn_cb(comch_connection);
}

void Server::new_consumer_cb(
    doca_comch_event_consumer *event,
    doca_comch_connection *comch_connection,
    uint32_t id) {
    auto *server = from_conn_to_server(comch_connection);
    server->custom_new_consumer_cb(comch_connection, id);
}

void Server::expired_consumer_cb(
    doca_comch_event_consumer *event,
    doca_comch_connection *comch_connection,
    uint32_t id) {
    auto *server = from_conn_to_server(comch_connection);
    server->custom_expired_consumer_cb(comch_connection, id);
}
}
