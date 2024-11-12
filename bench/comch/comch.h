#include <cstdint>
#include <doca_dev.h>
#include <optional>
#include <memory>

#include "docacxx/ctx/server.h"
#include "docacxx/ctx/client.h"
#include "docacxx/ctx/consumer.h"
#include "docacxx/ctx/producer.h"
#include "docacxx/mem.h"
#include "docacxx/task_context.h"

class SyncConsumer {
public:
    SyncConsumer(doca_comch_connection *peer_conn, doca_pe *conn_pe,
        docacxx::Mmap &mmap, doca_dev *dev, size_t recv_task_slot_num): m_recv_ctx(recv_task_slot_num) {
        docacxx::check_derr(doca_pe_create(&m_pe), "failed to create pe");
        docacxx::comch::Consumer::Cfg cfg;
        cfg.m_recv_task_slot_num = recv_task_slot_num;
        m_inner = std::make_unique<docacxx::comch::Consumer>(peer_conn, m_pe,
            conn_pe, mmap, dev, cfg);
        m_inner->set_recv_cb(m_recv_ctx.make_pipeline_submitter());
        m_inner->start_sync();
    }

    ~SyncConsumer() {
        m_inner->clean_sync();
        auto result = doca_pe_destroy(m_pe);
        if (result != DOCA_SUCCESS) {
            std::cerr << "failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    void recv(const std::vector<std::unique_ptr<docacxx::Buffer>> &bufs) {
        m_recv_ctx.reset();

        for (size_t i{}; i < bufs.size(); ++i) {
            m_recv_ctx.submit([&, i] {
                auto task = m_inner->alloc_recv_task(*bufs[i], nullptr);
                m_inner->submit(std::move(task));
            });
        }
        while (m_recv_ctx.complete_cnt() != bufs.size()) {
            doca_pe_progress(m_pe);
        }
    }

protected:
    doca_pe *m_pe{};
    std::unique_ptr<docacxx::comch::Consumer> m_inner;
    TaskContext m_recv_ctx;
};

class SyncProducer {
public:
    SyncProducer(doca_comch_connection *peer_conn, doca_dev *dev,
        size_t send_task_slot_num, size_t consumer_id)
        :m_send_ctx(send_task_slot_num), m_consumer_id{consumer_id} {
        docacxx::check_derr(doca_pe_create(&m_pe), "failed to create pe");
        docacxx::comch::Producer::Cfg cfg;
        cfg.m_send_task_slot_num = send_task_slot_num;
        m_inner = std::make_unique<docacxx::comch::Producer>(peer_conn, m_pe, dev, cfg);
        m_inner->set_send_cb(m_send_ctx.make_pipeline_submitter());
        m_inner->start_sync();
    }

    ~SyncProducer() {
        m_inner->clean_sync();
        auto result = doca_pe_destroy(m_pe);
        if (result != DOCA_SUCCESS) {
            std::cerr << "failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    void send(const std::vector<std::unique_ptr<docacxx::Buffer>> &bufs) {
        m_send_ctx.reset();

        for (size_t i{}; i < bufs.size(); ++i) {
            m_send_ctx.submit([&, i] {
                auto task = m_inner->alloc_send_task(*bufs[i], nullptr, 0, m_consumer_id, nullptr);
                auto ret = m_inner->submit(std::move(task));
                while (ret) {
                    doca_pe_progress(m_pe);
                    m_inner->submit(std::move(*ret));
                }
            });
        }
        while (m_send_ctx.complete_cnt() != bufs.size()) {
            doca_pe_progress(m_pe);
        }
    }

protected:
    doca_pe *m_pe{};
    std::unique_ptr<docacxx::comch::Producer> m_inner;
    TaskContext m_send_ctx;
    size_t m_consumer_id{};
};

class DpuEnd {
public:
    DpuEnd(std::string_view name, doca_dev *dev, doca_dev_rep *dev_rep,
        docacxx::Mmap &mmap, size_t send_task_slot_num, size_t recv_task_slot_num) {
        docacxx::check_derr(doca_pe_create(&m_pe), "failed to create pe");

        m_inner = std::make_unique<docacxx::comch::Server>(name.data(), dev, dev_rep, m_pe, docacxx::comch::Server::Cfg{});
        m_inner->set_new_conn_cb([&](doca_comch_connection *conn) {
            m_peer_conn = conn;
        });
        m_inner->set_new_consumer_cb([&](doca_comch_connection *conn, uint32_t id) {
            m_host_consumer_id = id;
        });
        m_inner->start_sync();

        while (not m_host_consumer_id) {
            doca_pe_progress(m_pe);
        }
        m_consumer = std::make_unique<SyncConsumer>(m_peer_conn, m_pe, mmap, dev, recv_task_slot_num);
        m_producer = std::make_unique<SyncProducer>(m_peer_conn, dev, send_task_slot_num, *m_host_consumer_id);
    }

    ~DpuEnd() {
        m_producer.reset();
        m_consumer.reset();

        m_inner->clean_sync();

        auto result = doca_pe_destroy(m_pe);
        if (result != DOCA_SUCCESS) {
            std::cerr << "failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    void send(const std::vector<std::unique_ptr<docacxx::Buffer>> &bufs) {
        m_producer->send(bufs);
    }
    void recv(const std::vector<std::unique_ptr<docacxx::Buffer>> &bufs) {
        m_consumer->recv(bufs);
    }

private:
    doca_pe *m_pe;
    std::unique_ptr<docacxx::comch::Server> m_inner;
    doca_comch_connection *m_peer_conn{};

    std::optional<size_t> m_host_consumer_id;
    std::unique_ptr<SyncProducer> m_producer;
    std::unique_ptr<SyncConsumer> m_consumer;
};

class HostEnd {
public:
    HostEnd(std::string_view server_name, doca_dev *dev,
        docacxx::Mmap &mmap, size_t send_task_slot_num, size_t recv_task_slot_num) {
        docacxx::check_derr(doca_pe_create(&m_pe), "failed to create pe");

        m_inner = std::make_unique<docacxx::comch::Client>(server_name.data(), dev, m_pe, docacxx::comch::Client::Cfg{});
        m_inner->set_new_consumer_cb([&](doca_comch_connection *conn, uint32_t id) {
            m_dpu_consumer_id = id;
        });
        m_inner->start_sync();

        m_peer_conn = m_inner->get_connection();

        m_consumer = std::make_unique<SyncConsumer>(m_peer_conn, m_pe, mmap, dev, recv_task_slot_num);
        while (not m_dpu_consumer_id) {
            doca_pe_progress(m_pe);
        }
        m_producer = std::make_unique<SyncProducer>(m_peer_conn, dev, send_task_slot_num, *m_dpu_consumer_id);
    }

    ~HostEnd() {
        m_producer.reset();
        m_consumer.reset();

        m_inner->clean_sync();

        auto result = doca_pe_destroy(m_pe);
        if (result != DOCA_SUCCESS) {
            std::cerr << "failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    void send(const std::vector<std::unique_ptr<docacxx::Buffer>> &bufs) {
        m_producer->send(bufs);
    }
    void recv(const std::vector<std::unique_ptr<docacxx::Buffer>> &bufs) {
        m_consumer->recv(bufs);
    }

private:
    doca_pe *m_pe;
    std::unique_ptr<docacxx::comch::Client> m_inner;
    doca_comch_connection *m_peer_conn{};

    std::optional<size_t> m_dpu_consumer_id;
    std::unique_ptr<SyncProducer> m_producer;
    std::unique_ptr<SyncConsumer> m_consumer;
};
