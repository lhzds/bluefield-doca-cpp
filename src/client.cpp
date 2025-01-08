#include "docacxx/doca_utils.h"
#include "docacxx/ctx/client.h"

#include <doca_ctx.h>
#include <iostream>

namespace docacxx {
namespace comch {
Client::Client(std::string_view server_name, doca_dev *dev, doca_pe *pe, const Cfg &cfg) {
    check_derr(doca_comch_client_create(dev, server_name.data(), &m_inner),
        "failed to create client");    
    init_ctx(doca_comch_client_as_ctx(m_inner), pe);

    check_derr(doca_comch_client_event_consumer_register(
            m_inner, new_consumer_cb, expired_consumer_cb),
        "failed to register consumer event");

    check_derr(doca_comch_client_task_send_set_conf(m_inner, send_cb, send_error_cb, cfg.max_send_task_slot_num),
        "failed to set send task conf");
    check_derr(doca_comch_client_event_msg_recv_register(m_inner, recv_cb),
        "failed to register recv msg event");

    // check_derr(doca_comch_client_set_max_msg_size(m_inner, 4080), "failed to
    // set max msg size");
    // check_derr(doca_comch_client_set_recv_queue_size(m_inner, 1024),
    //     "failed to set max queue size");
}

void Client::clean_impl() {
    auto result = doca_comch_client_destroy(m_inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy client properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
}

doca_comch_connection *Client::get_connection() {
    doca_comch_connection *conn{};
    check_derr(
        doca_comch_client_get_connection(m_inner, &conn),
        "failed to get connection from client");
    return conn;
}

const doca_comch_connection *Client::get_connection() const {
    return const_cast<Client *>(this)->get_connection();
}

std::unique_ptr<Client::SendTask> Client::alloc_send_task(doca_comch_connection *peer,
    const void *msg, uint32_t len, SendTask::CompleteCallback cb) {
    doca_comch_task_send *task{};
    check_derr(doca_comch_client_task_send_alloc_init(m_inner, peer, msg, len, &task),
        "failed to alloc comch send task");
    return std::make_unique<SendTask>(task, std::move(cb));
}

std::unique_ptr<Client::SendTask> Client::submit(std::unique_ptr<Client::SendTask> task) {
    auto *released = task.release();
    if (not submit_impl(released->raw_base())) {
        return std::unique_ptr<Client::SendTask>(released);
    }
    return {};
}

Client *Client::from_conn_to_client(doca_comch_connection *comch_connection) {
    auto *raw_client = doca_comch_client_get_client_ctx(comch_connection);
    doca_data ctx_data{};
    check_derr(
        doca_ctx_get_user_data(
            doca_comch_client_as_ctx(raw_client), &ctx_data),
        "failed to get user data of ctx");
    return (Client *)ctx_data.ptr;
}

void Client::send_cb(
    doca_comch_task_send *raw_send_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto task = std::unique_ptr<SendTask>((SendTask *)task_data.ptr);
    task->call_cb(); // 任务成功就会调用回调

    auto *client = (Client *)ctx_data.ptr;
    client->m_send_cb(std::move(task));
}

void Client::send_error_cb(
    doca_comch_task_send *raw_send_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto task = std::unique_ptr<SendTask>((SendTask *)task_data.ptr);

    auto *client = (Client *)ctx_data.ptr;
    client->m_send_error_cb(std::move(task));
}

void Client::recv_cb(
    doca_comch_event_msg_recv *,
    uint8_t *msg_addr,
    uint32_t msg_len,
    struct doca_comch_connection *comch_connection) {
    auto *client = from_conn_to_client(comch_connection);
    client->m_recv_cb(comch_connection, msg_addr, msg_len);
}

void Client::new_consumer_cb(
    doca_comch_event_consumer *event,
    doca_comch_connection *comch_connection,
    uint32_t id) {
    auto *client = from_conn_to_client(comch_connection);
    client->m_new_consumer_cb(comch_connection, id);
}

void Client::expired_consumer_cb(
    doca_comch_event_consumer *event,
    doca_comch_connection *comch_connection,
    uint32_t id) {
    auto *client = from_conn_to_client(comch_connection);
    client->m_expired_consumer_cb(comch_connection, id);
}
}
}
