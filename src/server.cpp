#include "docacxx/doca_utils.h"
#include "docacxx/ctx/server.h"

#include <doca_comch_producer.h>
#include <doca_error.h>
#include <iostream>

namespace docacxx {
namespace comch {
Server::Server(
    std::string_view name,
    doca_dev *dev, doca_dev_rep *dev_rep,
    doca_pe *pe, const Cfg &cfg) {
    check_derr(
        doca_comch_server_create(dev, dev_rep, name.data(), &m_inner),
        "failed to create server");
    init_ctx(doca_comch_server_as_ctx(m_inner), pe);

    check_derr(
        doca_comch_server_event_connection_status_changed_register(
            m_inner, new_conn_cb, disconn_cb),
        "failed to register connection event");
    check_derr(
        doca_comch_server_event_consumer_register(
            m_inner, new_consumer_cb, expired_consumer_cb),
        "failed to register consumer event");

    check_derr(doca_comch_server_task_send_set_conf(m_inner, send_cb, send_error_cb, cfg.max_send_task_slot_num),
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
}

std::unique_ptr<Server::SendTask> Server::alloc_send_task(doca_comch_connection *peer,
    const void *msg, uint32_t len, SendTask::CompleteCallback cb) {
    doca_comch_task_send *task{};
    check_derr(doca_comch_server_task_send_alloc_init(m_inner, peer, msg, len, &task),
        "failed to alloc comch send task");
    return std::make_unique<SendTask>(task, std::move(cb));
}

std::unique_ptr<Server::SendTask> Server::submit(std::unique_ptr<Server::SendTask> task) {
    auto *released = task.release();
    if (not submit_impl(released->raw_base())) {
        return std::unique_ptr<Server::SendTask>(released);
    }
    return {};
}

Server *Server::from_conn_to_server(doca_comch_connection *comch_connection) {
    auto *raw_server = doca_comch_server_get_server_ctx(comch_connection);
    doca_data ctx_data{};
    check_derr(
        doca_ctx_get_user_data(
            doca_comch_server_as_ctx(raw_server), &ctx_data),
        "failed to get user data of ctx");
    return (Server *)ctx_data.ptr;
}

void Server::send_cb(
    doca_comch_task_send *raw_send_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto task = std::unique_ptr<SendTask>((SendTask *)task_data.ptr);
    task->call_cb(); // 任务成功就会调用回调

    auto *server = (Server *)ctx_data.ptr;
    server->m_send_cb(std::move(task));
}

void Server::send_error_cb(
    doca_comch_task_send *raw_send_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto task = std::unique_ptr<SendTask>((SendTask *)task_data.ptr);

    auto *server = (Server *)ctx_data.ptr;
    server->m_send_error_cb(std::move(task));
}

void Server::recv_cb(
    doca_comch_event_msg_recv *,
    uint8_t *msg_addr,
    uint32_t msg_len,
    struct doca_comch_connection *comch_connection) {
    auto *server = from_conn_to_server(comch_connection);
    server->m_recv_cb(comch_connection, msg_addr, msg_len);
}

void Server::new_conn_cb(
    doca_comch_event_connection_status_changed *event,
    doca_comch_connection *comch_connection,
    uint8_t change_success) {
    auto *server = from_conn_to_server(comch_connection);
    server->m_new_conn_cb(comch_connection);
}

void Server::disconn_cb(
    doca_comch_event_connection_status_changed *event,
    doca_comch_connection *comch_connection,
    uint8_t change_success) {
    auto *server = from_conn_to_server(comch_connection);
    server->m_disconn_cb(comch_connection);
}

void Server::new_consumer_cb(
    doca_comch_event_consumer *event,
    doca_comch_connection *comch_connection,
    uint32_t id) {
    auto *server = from_conn_to_server(comch_connection);
    server->m_new_consumer_cb(comch_connection, id);
}

void Server::expired_consumer_cb(
    doca_comch_event_consumer *event,
    doca_comch_connection *comch_connection,
    uint32_t id) {
    auto *server = from_conn_to_server(comch_connection);
    server->m_expired_consumer_cb(comch_connection, id);
}
}
}
