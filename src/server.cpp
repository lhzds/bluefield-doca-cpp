#include "docacxx/doca_utils.h"
#include "docacxx/server.h"

#include <doca_comch_producer.h>
#include <doca_error.h>
#include <iostream>

namespace docacxx {
Server::Server(
    std::string_view name,
    doca_dev *dev, doca_dev_rep *dev_rep,
    doca_pe *pe, const Cfg &cfg) {
    check_derr(
        doca_comch_server_create(dev, dev_rep, name.data(), &m_inner),
        "failed to create server");
    init_ctx(doca_comch_server_as_ctx(m_inner), pe);

    check_derr(
        doca_comch_server_event_connection_register(
            m_inner, new_conn_cb, disconn_cb),
        "failed to register connection event");
    check_derr(
        doca_comch_server_event_consumer_register(
            m_inner, new_consumer_cb, expired_consumer_cb),
        "failed to register consumer event");

    check_derr(doca_comch_server_task_send_set_conf(m_inner, send_cb, send_error_cb, cfg.m_max_send_task_num),
        "failed to set send task conf");
    check_derr(doca_comch_server_event_msg_recv_register(m_inner, recv_cb),
        "failed to register recv msg event");

    // check_derr(doca_comch_server_set_max_msg_size(inner, cfg.max_msg_size),
    // "failed to set max msg size");
    // check_derr(doca_comch_server_set_recv_queue_size(inner,
    // cfg.max_recv_queue_size),
    //     "failed to set max queue size");
}

void Server::clean_impl() {
    auto result = doca_comch_server_destroy(m_inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy server properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
    result = doca_dev_close(m_dev);
    if (result != DOCA_SUCCESS)
        std::cerr << "Failed to close dev properly with error = "
                  << doca_error_get_name(result) << std::endl;
    result = doca_dev_rep_close(m_dev_rep);
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
    server->m_custom_send_error_cb();
}

void Server::send_error_cb(
    doca_comch_task_send *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_comch_task_send_as_task(task));
    auto *server = (Server *)ctx_user_data.ptr;
    server->m_custom_send_error_cb();
}

void Server::recv_cb(
    doca_comch_event_msg_recv *,
    uint8_t *recv_buffer,
    uint32_t msg_len,
    struct doca_comch_connection *comch_connection) {
    auto *server = from_conn_to_server(comch_connection);
    server->m_custom_recv_cb(recv_buffer, msg_len, comch_connection);
}

void Server::new_conn_cb(
    doca_comch_event_connection_status_changed *event,
    doca_comch_connection *comch_connection,
    uint8_t change_success) {
    auto *server = from_conn_to_server(comch_connection);
    server->m_custom_new_conn_cb(comch_connection);
}

void Server::disconn_cb(
    doca_comch_event_connection_status_changed *event,
    doca_comch_connection *comch_connection,
    uint8_t change_success) {
    auto *server = from_conn_to_server(comch_connection);
    server->m_custom_disconn_cb(comch_connection);
}

void Server::new_consumer_cb(
    doca_comch_event_consumer *event,
    doca_comch_connection *comch_connection,
    uint32_t id) {
    auto *server = from_conn_to_server(comch_connection);
    server->m_custom_new_consumer_cb(comch_connection, id);
}

void Server::expired_consumer_cb(
    doca_comch_event_consumer *event,
    doca_comch_connection *comch_connection,
    uint32_t id) {
    auto *server = from_conn_to_server(comch_connection);
    server->m_custom_expired_consumer_cb(comch_connection, id);
}
}
