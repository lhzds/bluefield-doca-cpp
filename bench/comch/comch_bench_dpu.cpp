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
std::string rep_pci_addr;
std::string output_file;
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
    program.add_argument("-p")
        .help("device rep pci address")
        .required();
    program.add_argument("-o")
        .help("output file")
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
    rep_pci_addr = program.get<std::string>("-p");
    output_file = program.get<std::string>("-o");
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
    std::cout << "Device rep pci address: " << rep_pci_addr << std::endl;
    std::cout << "Output file: " << output_file << std::endl;
    std::cout << "Msg size: " << msg_size << std::endl;
    std::cout << "Max send task num: " << max_send_task_num << std::endl;
    std::cout << "Max recv task num: " << max_recv_task_num << std::endl;
}

int main(int argc, char **argv) {
    read_args(argc, argv);
    
    // open device
    doca_dev *dev{};
    doca_dev_rep *dev_rep{};
    docacxx::check_derr(docacxx::open_doca_device_with_pci(dev_pci_addr.data(), &dev),
        "failed to open dev");
    docacxx::check_derr(docacxx::open_doca_device_rep_with_pci(dev, DOCA_DEVINFO_REP_FILTER_NET,
        rep_pci_addr.data(), &dev_rep), "failed to open dev");

    // create doca buf inv
    docacxx::BufInventory send_buf_inv(dev, max_send_task_num * msg_size,
        max_send_task_num);
    docacxx::BufInventory recv_buf_inv(dev, max_recv_task_num * msg_size,
        max_recv_task_num);
    for (size_t i{}; i < send_buf_inv.m_size; ++i) {
        send_buf_inv.m_mem[i] = i % 100;
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
    DpuEnd dpu_end(SERVER_NAME, dev, dev_rep, recv_buf_inv.m_mmap,
        max_send_task_num, max_recv_task_num);

    decltype(Timer::now()) start, end;

    // recv bench
    start = Timer::now();
    dpu_end.recv(recv_bufs);

    for (size_t i{1}; i < recv_repeat_time; ++i) {
        for (auto &recv_buf : recv_bufs) {
            recv_buf->clear_doca_buf_data();
        }
        dpu_end.recv(recv_bufs);
    }
    end = Timer::now();
    auto recv_time = duration_cast<Duration>(end - start);

    // send bench
    start = Timer::now();
    dpu_end.send(send_bufs);

    for (size_t i{1}; i < send_repeat_time; ++i) {
        dpu_end.send(send_bufs);
    }
    end = Timer::now();
    auto send_time = duration_cast<Duration>(end - start);

    // test
    if (std::string_view(send_buf_inv.m_mem, send_buf_inv.m_size) 
        != std::string_view(recv_buf_inv.m_mem, recv_buf_inv.m_size)) {
        std::cerr << "the contents are different" << std::endl;
        exit(EXIT_FAILURE);
    }

    // output_perf
    std::ofstream output_fs{output_file};
    if (not output_fs) {
        std::cerr << "Unable to open " << output_file << std::endl;
        exit(EXIT_FAILURE);   
    }

    Json json;
    json["msg_size"] = msg_size;
    
    json["send"]["task_num"] = max_send_task_num;
    json["send"]["count"] = max_send_task_num * send_repeat_time;
    json["send"]["time"] = duration_str(send_time);

    json["recv"]["task_num"] = max_recv_task_num;
    json["recv"]["count"] = max_recv_task_num * recv_repeat_time;
    json["recv"]["time"] = duration_str(recv_time);

    std::cout << json.dump(4) << std::endl;
    output_fs << json.dump(4) << std::endl;
    output_fs.close();

    return 0;
}
