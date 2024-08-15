#include "docacxx/server.h"
#include "docacxx/client.h"
#include "docacxx/consumer.h"
#include "docacxx/producer.h"
#include "docacxx/mem.h"
#include <doca_dev.h>
#include <optional>
#include <queue>

class SyncConsumer {
public:
    SyncConsumer(doca_comch_connection *peer_conn, doca_pe *conn_pe,
        doca_mmap *mmap, doca_dev *dev, size_t doca_max_recv_task_num)
        :doca_max_recv_task_num_{doca_max_recv_task_num} {
        docacxx::check_derr(doca_pe_create(&pe_), "failed to create pe");

        docacxx::Consumer::Cfg cfg;
        cfg.recv_task_num = doca_max_recv_task_num;

        inner_ = std::make_unique<docacxx::Consumer>(peer_conn, pe_,
            conn_pe, mmap, dev, cfg);
        inner_->custom_recv_cb = [&]() {
            if (not wait_queue_.empty()) {
                auto buf = wait_queue_.front();
                wait_queue_.pop();
                auto *task = inner_->alloc_recv_task(buf);
                inner_->submit(task);
            }

            ++finished_;
        };

        inner_->start();
        while (inner_->state != DOCA_CTX_STATE_RUNNING) {
            doca_pe_progress(pe_);
            doca_pe_progress(conn_pe);
        }
    }

    ~SyncConsumer() {
        inner_->stop();
        while (inner_->state != DOCA_CTX_STATE_IDLE) {
            doca_pe_progress(pe_);
        }
        inner_->clean();

        auto result = doca_pe_destroy(pe_);
        if (result != DOCA_SUCCESS) {
            std::cerr << "failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    void recv(const std::vector<std::unique_ptr<docacxx::Buffer>> &bufs) {
        finished_ = 0;

        for (size_t i{}; i < std::min(bufs.size(), doca_max_recv_task_num_); ++i) {
            auto *task = inner_->alloc_recv_task(bufs[i]->buf);
            inner_->submit(task);
        }

        if (bufs.size() > doca_max_recv_task_num_) {
            for (size_t i = doca_max_recv_task_num_; i < bufs.size(); ++i) {
                wait_queue_.emplace(bufs[i]->buf);
            }
        }
        while (finished_ != bufs.size()) {
            doca_pe_progress(pe_);
        }
    }

protected:
    doca_pe *pe_{};
    size_t doca_max_recv_task_num_{};
    std::unique_ptr<docacxx::Consumer> inner_;
    size_t finished_{};
    std::queue<doca_buf *> wait_queue_;
};

class SyncProducer {
public:
    SyncProducer(doca_comch_connection *peer_conn, doca_dev *dev,
        size_t doca_max_send_task_num, size_t consumer_id)
        :doca_max_send_task_num_{doca_max_send_task_num}, consumer_id_{consumer_id} {
        docacxx::check_derr(doca_pe_create(&pe_), "failed to create pe");

        docacxx::Producer::Cfg cfg;
        cfg.send_task_num = doca_max_send_task_num;

        inner_ = std::make_unique<docacxx::Producer>(peer_conn, pe_, dev, cfg);
        inner_->custom_send_cb = [&]() {
            if (not wait_queue_.empty()) {
                auto buf = wait_queue_.front();
                wait_queue_.pop();
                auto *task = inner_->alloc_send_task(buf, nullptr, 0, consumer_id_);
                while (not inner_->submit(task)) {
                    doca_pe_progress(pe_);
                }
            }

            ++finished_;
        };

        inner_->start();
        while (inner_->state != DOCA_CTX_STATE_RUNNING) {
            doca_pe_progress(pe_);
        }
    }

    ~SyncProducer() {
        inner_->stop();
        while (inner_->state != DOCA_CTX_STATE_IDLE) {
            doca_pe_progress(pe_);
        }
        inner_->clean();

        auto result = doca_pe_destroy(pe_);
        if (result != DOCA_SUCCESS) {
            std::cerr << "failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    void send(const std::vector<std::unique_ptr<docacxx::Buffer>> &bufs) {
        finished_ = 0;

        for (size_t i{}; i < std::min(bufs.size(), doca_max_send_task_num_); ++i) {
            auto *task = inner_->alloc_send_task(bufs[i]->buf, nullptr, 0, consumer_id_);
            while (not inner_->submit(task)) {
                doca_pe_progress(pe_);
            }
        }

        if (bufs.size() > doca_max_send_task_num_) {
            for (size_t i = doca_max_send_task_num_; i < bufs.size(); ++i) {
                wait_queue_.emplace(bufs[i]->buf);
            }
        }
        while (finished_ != bufs.size()) {
            doca_pe_progress(pe_);
        }
    }

protected:
    doca_pe *pe_{};
    size_t doca_max_send_task_num_{};
    std::unique_ptr<docacxx::Producer> inner_;
    size_t finished_{};
    size_t consumer_id_{};
    std::queue<doca_buf *> wait_queue_;
};

class DpuEnd {
public:
    DpuEnd(std::string_view name, doca_dev *dev, doca_dev_rep *dev_rep,
        doca_mmap *recv_mmap, size_t doca_max_send_task_num, size_t doca_max_recv_task_num)
        :recv_mmap_{recv_mmap}, doca_max_send_task_num_{doca_max_send_task_num}, doca_max_recv_task_num_{doca_max_recv_task_num} {
        docacxx::check_derr(doca_pe_create(&pe_), "failed to create pe");

        inner_ = std::make_unique<docacxx::Server>(name.data(), dev, dev_rep, pe_, docacxx::Server::Cfg{});
        
        inner_->custom_new_conn_cb = [&](doca_comch_connection *conn) {
            peer_conn_ = conn;
        };

        inner_->custom_new_consumer_cb = [&](doca_comch_connection *conn, uint32_t id) {
            host_consumer_id_ = id;
        };

        inner_->start();
        while (inner_->state != DOCA_CTX_STATE_RUNNING) {
            doca_pe_progress(pe_);
        }

        while (not host_consumer_id_) {
            doca_pe_progress(pe_);
        }
        consumer_ = std::make_unique<SyncConsumer>(peer_conn_, pe_, recv_mmap_, dev, doca_max_recv_task_num);
        producer_ = std::make_unique<SyncProducer>(peer_conn_, dev, doca_max_send_task_num, *host_consumer_id_);
    }

    ~DpuEnd() {
        producer_.reset();
        consumer_.reset();

        inner_->stop();
        while (inner_->state != DOCA_CTX_STATE_IDLE) {
            doca_pe_progress(pe_);
        }
        inner_->clean();

        auto result = doca_pe_destroy(pe_);
        if (result != DOCA_SUCCESS) {
            std::cerr << "failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    void send(const std::vector<std::unique_ptr<docacxx::Buffer>> &bufs) {
        producer_->send(bufs);
    }
    void recv(const std::vector<std::unique_ptr<docacxx::Buffer>> &bufs) {
        consumer_->recv(bufs);
    }

private:
    doca_mmap *recv_mmap_;
    size_t doca_max_send_task_num_;
    size_t doca_max_recv_task_num_;

    doca_pe *pe_;
    std::unique_ptr<docacxx::Server> inner_;
    doca_comch_connection *peer_conn_{};

    std::optional<size_t> host_consumer_id_;
    std::unique_ptr<SyncProducer> producer_;
    std::unique_ptr<SyncConsumer> consumer_;
};

class HostEnd {
public:
    HostEnd(std::string_view server_name, doca_dev *dev,
        doca_mmap *recv_mmap, size_t doca_max_send_task_num, size_t doca_max_recv_task_num)
        :recv_mmap_{recv_mmap}, doca_max_send_task_num_{doca_max_send_task_num}, doca_max_recv_task_num_{doca_max_recv_task_num} {
        docacxx::check_derr(doca_pe_create(&pe_), "failed to create pe");

        inner_ = std::make_unique<docacxx::Client>(server_name.data(), dev, pe_, docacxx::Client::Cfg{});

        inner_->custom_new_consumer_cb = [&](doca_comch_connection *conn, uint32_t id) {
            dpu_consumer_id_ = id;
        };

        inner_->start();
        while (inner_->state != DOCA_CTX_STATE_RUNNING) {
            doca_pe_progress(pe_);
        }

        peer_conn_ = inner_->get_connection();

        consumer_ = std::make_unique<SyncConsumer>(peer_conn_, pe_, recv_mmap_, dev, doca_max_recv_task_num);
        while (not dpu_consumer_id_) {
            doca_pe_progress(pe_);
        }
        producer_ = std::make_unique<SyncProducer>(peer_conn_, dev, doca_max_send_task_num, *dpu_consumer_id_);
    }

    ~HostEnd() {
        producer_.reset();
        consumer_.reset();

        inner_->stop();
        while (inner_->state != DOCA_CTX_STATE_IDLE) {
            doca_pe_progress(pe_);
        }
        inner_->clean();

        auto result = doca_pe_destroy(pe_);
        if (result != DOCA_SUCCESS) {
            std::cerr << "failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    void send(const std::vector<std::unique_ptr<docacxx::Buffer>> &bufs) {
        producer_->send(bufs);
    }
    void recv(const std::vector<std::unique_ptr<docacxx::Buffer>> &bufs) {
        consumer_->recv(bufs);
    }

private:
    doca_mmap *recv_mmap_;
    size_t doca_max_send_task_num_;
    size_t doca_max_recv_task_num_;

    doca_pe *pe_;
    std::unique_ptr<docacxx::Client> inner_;
    doca_comch_connection *peer_conn_{};

    std::optional<size_t> dpu_consumer_id_;
    std::unique_ptr<SyncProducer> producer_;
    std::unique_ptr<SyncConsumer> consumer_;
};
