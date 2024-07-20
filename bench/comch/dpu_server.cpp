#include "docacxx/server.h"
#include "docacxx/consumer.h"
#include "docacxx/producer.h"
#include "../bench_util.h"
#include <chrono>

constexpr const char *kDevPci{"03:00.1"};
constexpr const char *kRepPci{"cb:00.1"};
constexpr const char *kServerName{"foo"};
constexpr size_t kLowerBound{4 * KB};
constexpr size_t kUpperBound{2 * MB};
constexpr size_t kCntOneRound{10};

using namespace std::chrono;
using Timer = std::chrono::high_resolution_clock;

struct BenchServer {
    Server inner;
    doca_comch_connection *peer_conn{};

    bool is_client_consumer_set{};
    uint32_t client_consumer{};
    Producer *producer{};
    Consumer *consumer{};
    
    BenchServer();
    ~BenchServer();
    
    void shake();
};

BenchServer::BenchServer(): inner(kServerName, kDevPci, kRepPci) {
    inner.set_custom_new_conn_cb([&](doca_comch_connection *conn) {
        peer_conn = conn;
        fmt::print("accept new connection.\n");
    });
    inner.set_custom_new_consumer_cb([&](doca_comch_connection *conn, uint32_t id) {
        client_consumer = id;
        is_client_consumer_set = true;
        fmt::print("new consumer is created.\n");
    });

}

BenchServer::~BenchServer() {
    if (producer) {
        delete producer;
    }
    if (consumer) {
        delete consumer;
    }
}

void BenchServer::shake() {
    while (not is_client_consumer_set) {
        doca_pe_progress(inner.pe);
    }
    consumer = new Consumer(peer_conn, inner.pe, inner.dev, Consumer::Cfg{
        kUpperBound
    });
    producer = new Producer(peer_conn, inner.dev, Producer::Cfg{
        kUpperBound
    });
}

int main() {
    BenchServer s;
    s.shake();
    for (size_t msg_sz{kLowerBound}; msg_sz <= kUpperBound; msg_sz *= 2) {
        for (int i{}; i < kCntOneRound; ++i) {
            s.consumer->recv_block();
        }
    }

    fmt::print("from dpu to host\n");
    std::vector<char> data_to_send(kUpperBound);
    for (size_t msg_sz{kLowerBound}; msg_sz <= kUpperBound; msg_sz *= 2) {
        auto start = Timer::now();
        for (int i{}; i < kCntOneRound; ++i) {
            s.producer->send_block({data_to_send.data(), msg_sz}, s.client_consumer);
        }
        auto end = Timer::now();
        auto lat_us = duration_cast<microseconds>(end - start).count() / 10;
        fmt::print("{}: {}us\n", size_str(msg_sz), lat_us);
    }
    return 0;
}
