#include <argparse/argparse.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include "comch.h"
#include "docacxx/mem.h"
#include "scoped_guard.h"

const char *SERVER_NAME{"bench_server"};

std::string dev_pci_addr;
size_t msg_size;
size_t comch_task_slot_num;

size_t repeat_time;

void read_args(int argc, char **argv) {
    argparse::ArgumentParser program("ec_bench");
    
    program.add_argument("-a")
        .help("device pci address")
        .required();
    program.add_argument("-b")
        .help("msg size")
        .required()
        .scan<'d', size_t>();
    program.add_argument("-c")
        .help("max comch task num")
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
    comch_task_slot_num = program.get<size_t>("-c");

    if (comch_task_slot_num >= 128) {
        repeat_time = 1;
    } else {
        repeat_time = 128 / comch_task_slot_num + 1;
    }

    std::cout << "Device pci address: " << dev_pci_addr << std::endl;
    std::cout << "Msg size: " << msg_size << std::endl;
    std::cout << "Comch task slot num: " << comch_task_slot_num << std::endl;
}

int main(int argc, char **argv) {
    read_args(argc, argv);
    
    // open device
    doca_dev *dev{};
    docacxx::check_derr(docacxx::open_doca_device_with_pci(dev_pci_addr.data(), &dev),
            "failed to open dev");
    ScopeGuard guard([&] {
        docacxx::check_derr(doca_dev_close(dev), "failed to close dev");
    });

    // create doca buf inv
    docacxx::BufInventoryWithMmap send_buf_inv(dev, comch_task_slot_num * msg_size,
        comch_task_slot_num);
    docacxx::BufInventoryWithMmap recv_buf_inv(dev, comch_task_slot_num * msg_size,
        comch_task_slot_num);
    for (size_t i{}; i < send_buf_inv.size(); ++i) {
        send_buf_inv.data()[i] = i % 100;
    }

    // create buffers
    std::vector<std::unique_ptr<docacxx::Buffer>> send_bufs;
    send_bufs.reserve(comch_task_slot_num);
    for (size_t i{}; i < comch_task_slot_num; ++i) {
        auto buf_len = msg_size;
        send_bufs.emplace_back(send_buf_inv.get_buf(i * buf_len, buf_len));
        send_bufs.back()->set_doca_buf_data(buf_len);
    }

    std::vector<std::unique_ptr<docacxx::Buffer>> recv_bufs;
    recv_bufs.reserve(comch_task_slot_num);
    for (size_t i{}; i < comch_task_slot_num; ++i) {
        auto buf_len = msg_size;
        recv_bufs.emplace_back(recv_buf_inv.get_buf(i * buf_len, buf_len));
    }

    // create context
    HostEnd host_end(SERVER_NAME, dev, recv_buf_inv.mmap(), comch_task_slot_num, comch_task_slot_num);

    // bench
    host_end.recv(recv_bufs);
    host_end.send(send_bufs);
    for (size_t i{1}; i < repeat_time; ++i) {
        for (auto &dst_buf : recv_bufs) {
            dst_buf->clear_doca_buf_data();
        }
        host_end.recv(recv_bufs);
        host_end.send(send_bufs);
    }
    
    return 0;
}
