#include "comch.h"
#include "docacxx/mem.h"
#include "../part_iterator.h"
#include "../print_util.h"
#include <argparse/argparse.hpp>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using namespace std::chrono;
using Timer = std::chrono::high_resolution_clock;
using Duration = std::chrono::microseconds;
using Json = nlohmann::json;

const char *SERVER_NAME{"bench_server"};

std::string dev_pci_addr;
size_t msg_size;
size_t max_send_task_num;
size_t max_recv_task_num;

size_t send_repeat_time;
size_t recv_repeat_time;

void read_args(int argc, char **argv) {
    argparse::ArgumentParser program("ec_bench");
    
    program.add_argument("-a")
        .help("device pci address")
        .required();
    program.add_argument("-b")
        .help("msg size")
        .required()
        .scan<'d', size_t>();
    program.add_argument("-s")
        .help("max send task num")
        .required()
        .scan<'d', size_t>();
    program.add_argument("-r")
        .help("max recv task num")
        .required()
        .scan<'d', size_t>();

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    dev_pci_addr = program.get<std::string>("-a");
    msg_size = program.get<size_t>("-b");
    max_send_task_num = program.get<size_t>("-s");
    max_recv_task_num = program.get<size_t>("-r");

    if (max_send_task_num >= 128) {
        send_repeat_time = 1;
    } else {
        send_repeat_time = 128 / max_send_task_num + 1;
    }

    if (max_recv_task_num >= 128) {
        recv_repeat_time = 1;
    } else {
        recv_repeat_time = 128 / max_recv_task_num + 1;
    }

    std::cout << "Device pci address: " << dev_pci_addr << std::endl;
    std::cout << "Msg size: " << msg_size << std::endl;
    std::cout << "Max send task num: " << max_send_task_num << std::endl;
    std::cout << "Max recv task num: " << max_recv_task_num << std::endl;
}

int main(int argc, char **argv) {
    read_args(argc, argv);
    
    // open device
    doca_dev *dev{};
    docacxx::check_derr(docacxx::open_doca_device_with_pci(dev_pci_addr.data(), &dev),
            "failed to open dev");

    // create doca buf inv
    docacxx::BufInventory send_buf_inv(dev, max_send_task_num * msg_size,
        max_send_task_num);
    docacxx::BufInventory recv_buf_inv(dev, max_recv_task_num * msg_size,
        max_recv_task_num);
    for (size_t i{}; i < send_buf_inv.len; ++i) {
        send_buf_inv.mem[i] = i % 100;
    }

    // create buffers
    std::vector<std::unique_ptr<docacxx::Buffer>> send_bufs;
    send_bufs.reserve(max_send_task_num);
    for (size_t i{}; i < max_send_task_num; ++i) {
        auto buf_len = msg_size;
        send_bufs.emplace_back(send_buf_inv.get_buf(i * buf_len, buf_len));
        send_bufs.back()->set_doca_buf_data(buf_len);
    }

    std::vector<std::unique_ptr<docacxx::Buffer>> recv_bufs;
    recv_bufs.reserve(max_recv_task_num);
    for (size_t i{}; i < max_recv_task_num; ++i) {
        auto buf_len = msg_size;
        recv_bufs.emplace_back(recv_buf_inv.get_buf(i * buf_len, buf_len));
    }

    // create context
    HostEnd host_end(SERVER_NAME, dev, recv_buf_inv.mmap, max_send_task_num, max_recv_task_num);

    // send bench
    host_end.send(send_bufs);

    for (size_t i{1}; i < send_repeat_time; ++i) {
        for (auto &send_buf : send_bufs) {
            send_buf->clear_doca_buf_data();
        }
        host_end.send(send_bufs);
    }

    // recv bench
    host_end.recv(recv_bufs);

    for (size_t i{1}; i < recv_repeat_time; ++i) {
        for (auto &dst_buf : recv_bufs) {
            dst_buf->clear_doca_buf_data();
        }
        host_end.recv(recv_bufs);
    }

    return 0;
}
