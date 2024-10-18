#include "comch.h"
#include "docacxx/mem.h"
#include "../print_util.h"
#include <argparse/argparse.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <fstream>

using namespace std::chrono;
using Timer = std::chrono::high_resolution_clock;
using Duration = std::chrono::microseconds;
using Json = nlohmann::json;

const char *SERVER_NAME{"bench_server"};

std::string dev_pci_addr;
std::string rep_pci_addr;
std::string output_file;
size_t msg_size;
size_t max_task_num;

size_t repeat_time;

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
    rep_pci_addr = program.get<std::string>("-p");
    output_file = program.get<std::string>("-o");
    msg_size = program.get<size_t>("-b");
    max_task_num = program.get<size_t>("-c");

    if (max_task_num >= 128) {
        repeat_time = 1;
    } else {
        repeat_time = 128 / max_task_num + 1;
    }

    std::cout << "Device pci address: " << dev_pci_addr << std::endl;
    std::cout << "Device rep pci address: " << rep_pci_addr << std::endl;
    std::cout << "Output file: " << output_file << std::endl;
    std::cout << "Msg size: " << msg_size << std::endl;
    std::cout << "Max comch task num: " << max_task_num << std::endl;
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
    docacxx::BufInventory send_buf_inv(dev, max_task_num * msg_size,
        max_task_num);
    docacxx::BufInventory recv_buf_inv(dev, max_task_num * msg_size,
        max_task_num);
    for (size_t i{}; i < send_buf_inv.size(); ++i) {
        send_buf_inv.data()[i] = i % 100;
    }

    // create buffers
    std::vector<std::unique_ptr<docacxx::Buffer>> send_bufs;
    send_bufs.reserve(max_task_num);
    for (size_t i{}; i < max_task_num; ++i) {
        auto buf_len = msg_size;
        send_bufs.emplace_back(send_buf_inv.get_buf(i * buf_len, buf_len));
        send_bufs.back()->set_doca_buf_data(buf_len);
    }

    std::vector<std::unique_ptr<docacxx::Buffer>> recv_bufs;
    recv_bufs.reserve(max_task_num);
    for (size_t i{}; i < max_task_num; ++i) {
        auto buf_len = msg_size;
        recv_bufs.emplace_back(recv_buf_inv.get_buf(i * buf_len, buf_len));
    }

    // create context
    DpuEnd dpu_end(SERVER_NAME, dev, dev_rep, recv_buf_inv.raw_mmap(),
        max_task_num, max_task_num);

    decltype(Timer::now()) start, end;

    // bench
    start = Timer::now();
    dpu_end.send(send_bufs);
    dpu_end.recv(recv_bufs);

    for (size_t i{1}; i < repeat_time; ++i) {
        for (auto &recv_buf : recv_bufs) {
            recv_buf->clear_doca_buf_data();
        }

        dpu_end.send(send_bufs);
        dpu_end.recv(recv_bufs);
    }
    end = Timer::now();
    auto time = duration_cast<Duration>(end - start);

    // test
    if (std::string_view(send_buf_inv.data(), send_buf_inv.size()) 
        != std::string_view(recv_buf_inv.data(), recv_buf_inv.size())) {
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
    json["task_num"] = max_task_num;
    json["count"] = max_task_num * repeat_time;
    json["time"] = duration_str(time);

    std::cout << json.dump(4) << std::endl;
    output_fs << json.dump(4) << std::endl;
    output_fs.close();

    // close dev
    return 0;
}
