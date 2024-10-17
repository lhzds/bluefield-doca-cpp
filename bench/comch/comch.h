#include "docacxx/server.h"
#include "docacxx/client.h"
#include "docacxx/consumer.h"
#include "docacxx/producer.h"
#include "docacxx/mem.h"
#include <doca_dev.h>
#include <optional>
#include <queue>
#include <memory>

class SyncConsumer {
public:
    SyncConsumer(doca_comch_connection *peer_conn, doca_pe *conn_pe,
        doca_mmap *mmap, doca_dev *dev, size_t doca_max_recv_task_num)
        :m_doca_max_recv_task_num{doca_max_recv_task_num} {
        docacxx::check_derr(doca_pe_create(&m_pe), "failed to create pe");

        docacxx::comch::Consumer::Cfg cfg;
        cfg.m_recv_task_num = doca_max_recv_task_num;

        m_inner = std::make_unique<docacxx::comch::Consumer>(peer_conn, m_pe,
            conn_pe, mmap, dev, cfg);
        m_inner->m_custom_recv_cb = [&]() {
            if (not m_wait_queue.empty()) {
                auto buf = m_wait_queue.front();
                m_wait_queue.pop();
                auto *task = m_inner->alloc_recv_task(buf);
                m_inner->submit(task);
            }

            ++m_finished;
        };

        m_inner->start();
        while (m_inner->m_state != DOCA_CTX_STATE_RUNNING) {
            doca_pe_progress(m_pe);
            doca_pe_progress(conn_pe);
        }
    }

    ~SyncConsumer() {
        m_inner->stop();
        while (m_inner->m_state != DOCA_CTX_STATE_IDLE) {
            doca_pe_progress(m_pe);
        }
        m_inner->clean();

        auto result = doca_pe_destroy(m_pe);
        if (result != DOCA_SUCCESS) {
            std::cerr << "failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    void recv(const std::vector<std::unique_ptr<docacxx::Buffer>> &bufs) {
        m_finished = 0;

        for (size_t i{}; i < std::min(bufs.size(), m_doca_max_recv_task_num); ++i) {
            auto *task = m_inner->alloc_recv_task(bufs[i]->m_inner);
            m_inner->submit(task);
        }

        if (bufs.size() > m_doca_max_recv_task_num) {
            for (size_t i = m_doca_max_recv_task_num; i < bufs.size(); ++i) {
                m_wait_queue.emplace(bufs[i]->m_inner);
            }
        }
        while (m_finished != bufs.size()) {
            doca_pe_progress(m_pe);
        }
    }

protected:
    doca_pe *m_pe{};
    size_t m_doca_max_recv_task_num{};
    std::unique_ptr<docacxx::comch::Consumer> m_inner;
    size_t m_finished{};
    std::queue<doca_buf *> m_wait_queue;
};

class SyncProducer {
public:
    SyncProducer(doca_comch_connection *peer_conn, doca_dev *dev,
        size_t doca_max_send_task_num, size_t consumer_id)
        :m_doca_max_send_task_num{doca_max_send_task_num}, m_consumer_id{consumer_id} {
        docacxx::check_derr(doca_pe_create(&m_pe), "failed to create pe");

        docacxx::comch::Producer::Cfg cfg;
        cfg.m_send_task_num = doca_max_send_task_num;

        m_inner = std::make_unique<docacxx::comch::Producer>(peer_conn, m_pe, dev, cfg);
        m_inner->m_custom_send_cb = [&]() {
            if (not m_wait_queue.empty()) {
                auto buf = m_wait_queue.front();
                m_wait_queue.pop();
                auto *task = m_inner->alloc_send_task(buf, nullptr, 0, m_consumer_id);
                while (not m_inner->submit(task)) {
                    doca_pe_progress(m_pe);
                }
            }

            ++m_finished;
        };

        m_inner->start();
        while (m_inner->m_state != DOCA_CTX_STATE_RUNNING) {
            doca_pe_progress(m_pe);
        }
    }

    ~SyncProducer() {
        m_inner->stop();
        while (m_inner->m_state != DOCA_CTX_STATE_IDLE) {
            doca_pe_progress(m_pe);
        }
        m_inner->clean();

        auto result = doca_pe_destroy(m_pe);
        if (result != DOCA_SUCCESS) {
            std::cerr << "failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    void send(const std::vector<std::unique_ptr<docacxx::Buffer>> &bufs) {
        m_finished = 0;

        for (size_t i{}; i < std::min(bufs.size(), m_doca_max_send_task_num); ++i) {
            auto *task = m_inner->alloc_send_task(bufs[i]->m_inner, nullptr, 0, m_consumer_id);
            while (not m_inner->submit(task)) {
                doca_pe_progress(m_pe);
            }
        }

        if (bufs.size() > m_doca_max_send_task_num) {
            for (size_t i = m_doca_max_send_task_num; i < bufs.size(); ++i) {
                m_wait_queue.emplace(bufs[i]->m_inner);
            }
        }
        while (m_finished != bufs.size()) {
            doca_pe_progress(m_pe);
        }
    }

protected:
    doca_pe *m_pe{};
    size_t m_doca_max_send_task_num{};
    std::unique_ptr<docacxx::comch::Producer> m_inner;
    size_t m_finished{};
    size_t m_consumer_id{};
    std::queue<doca_buf *> m_wait_queue;
};

class DpuEnd {
public:
    DpuEnd(std::string_view name, doca_dev *dev, doca_dev_rep *dev_rep,
        doca_mmap *recv_mmap, size_t doca_max_send_task_num, size_t doca_max_recv_task_num)
        :m_recv_mmap{recv_mmap}, m_doca_max_send_task_num{doca_max_send_task_num}, m_doca_max_recv_task_num{doca_max_recv_task_num} {
        docacxx::check_derr(doca_pe_create(&m_pe), "failed to create pe");

        m_inner = std::make_unique<docacxx::comch::Server>(name.data(), dev, dev_rep, m_pe, docacxx::comch::Server::Cfg{});
        
        m_inner->m_custom_new_conn_cb = [&](doca_comch_connection *conn) {
            m_peer_conn = conn;
        };

        m_inner->m_custom_new_consumer_cb = [&](doca_comch_connection *conn, uint32_t id) {
            m_host_consumer_id = id;
        };

        m_inner->start();
        while (m_inner->m_state != DOCA_CTX_STATE_RUNNING) {
            doca_pe_progress(m_pe);
        }

        while (not m_host_consumer_id) {
            doca_pe_progress(m_pe);
        }
        m_consumer = std::make_unique<SyncConsumer>(m_peer_conn, m_pe, m_recv_mmap, dev, doca_max_recv_task_num);
        m_producer = std::make_unique<SyncProducer>(m_peer_conn, dev, doca_max_send_task_num, *m_host_consumer_id);
    }

    ~DpuEnd() {
        m_producer.reset();
        m_consumer.reset();

        m_inner->stop();
        while (m_inner->m_state != DOCA_CTX_STATE_IDLE) {
            doca_pe_progress(m_pe);
        }
        m_inner->clean();

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
    doca_mmap *m_recv_mmap;
    size_t m_doca_max_send_task_num;
    size_t m_doca_max_recv_task_num;

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
        doca_mmap *recv_mmap, size_t doca_max_send_task_num, size_t doca_max_recv_task_num)
        :m_recv_mmap{recv_mmap}, m_doca_max_send_task_num{doca_max_send_task_num}, m_doca_max_recv_task_num{doca_max_recv_task_num} {
        docacxx::check_derr(doca_pe_create(&m_pe), "failed to create pe");

        m_inner = std::make_unique<docacxx::comch::Client>(server_name.data(), dev, m_pe, docacxx::comch::Client::Cfg{});

        m_inner->m_custom_new_consumer_cb = [&](doca_comch_connection *conn, uint32_t id) {
            m_dpu_consumer_id = id;
        };

        m_inner->start();
        while (m_inner->m_state != DOCA_CTX_STATE_RUNNING) {
            doca_pe_progress(m_pe);
        }

        m_peer_conn = m_inner->get_connection();

        m_consumer = std::make_unique<SyncConsumer>(m_peer_conn, m_pe, m_recv_mmap, dev, doca_max_recv_task_num);
        while (not m_dpu_consumer_id) {
            doca_pe_progress(m_pe);
        }
        m_producer = std::make_unique<SyncProducer>(m_peer_conn, dev, doca_max_send_task_num, *m_dpu_consumer_id);
    }

    ~HostEnd() {
        m_producer.reset();
        m_consumer.reset();

        m_inner->stop();
        while (m_inner->m_state != DOCA_CTX_STATE_IDLE) {
            doca_pe_progress(m_pe);
        }
        m_inner->clean();

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
    doca_mmap *m_recv_mmap;
    size_t m_doca_max_send_task_num;
    size_t m_doca_max_recv_task_num;

    doca_pe *m_pe;
    std::unique_ptr<docacxx::comch::Client> m_inner;
    doca_comch_connection *m_peer_conn{};

    std::optional<size_t> m_dpu_consumer_id;
    std::unique_ptr<SyncProducer> m_producer;
    std::unique_ptr<SyncConsumer> m_consumer;
};
