#include "docacxx/client.h"
#include "docacxx/consumer.h"
#include "docacxx/producer.h"
#include "../bench_util.h"
#include <chrono>
#include <fmt/core.h>

constexpr const char *kDevPci{"cb:00.1"};
constexpr const char *kServerName{"foo"};
constexpr size_t kLowerBound{4 * KB};
constexpr size_t kUpperBound{2 * MB};
constexpr size_t kCntOneRound{10};

using namespace std::chrono;
using Timer = std::chrono::high_resolution_clock;

struct BenchClient {
    Client inner;
    doca_comch_connection *peer_conn{};

    bool is_server_consumer_set{};
    uint32_t server_consumer{};
    Producer *producer{};
    Consumer *consumer{};
    
    BenchClient();
    ~BenchClient();
    
    void shake();
};

BenchClient::BenchClient(): inner(kServerName, kDevPci) {
    inner.set_custom_new_consumer_cb([&](doca_comch_connection *conn, uint32_t id) {
        server_consumer = id;
        is_server_consumer_set = true;
    });

}

BenchClient::~BenchClient() {
    if (producer) {
        delete producer;
    }
    if (consumer) {
        delete consumer;
    }
}

void BenchClient::shake() {
    peer_conn = inner.get_connection();
    consumer = new Consumer(peer_conn, inner.pe, inner.dev, Consumer::Cfg{
        kUpperBound
    });
    while (not is_server_consumer_set) {
        doca_pe_progress(inner.pe);
    }
    producer = new Producer(peer_conn, inner.dev, Producer::Cfg{
        kUpperBound
    });
}

int main() {
    BenchClient c;
    c.shake();
    std::vector<char> data_to_send(kUpperBound);

    fmt::print("from host to dpu\n");
    for (size_t msg_sz{kLowerBound}; msg_sz <= kUpperBound; msg_sz *= 2) {
        auto start = Timer::now();
        for (int i{}; i < kCntOneRound; ++i) {
            c.producer->send_block({data_to_send.data(), msg_sz}, c.server_consumer);
        }
        auto end = Timer::now();
        auto lat_us = duration_cast<microseconds>(end - start).count() / 10;
        fmt::print("{}: {}us\n", size_str(msg_sz), lat_us);
    }

    for (size_t msg_sz{kLowerBound}; msg_sz <= kUpperBound; msg_sz *= 2) {
        for (int i{}; i < kCntOneRound; ++i) {
            c.consumer->recv_block();
        }
    }
    return 0;
}
