#include "erasure_coder.h"
#include "docacxx/mem.h"
#include "../part_iterator.h"
#include "../print_util.h"
#include <argparse/argparse.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using namespace std::chrono;
using Timer = std::chrono::high_resolution_clock;
using Duration = std::chrono::microseconds;
using Json = nlohmann::json;

constexpr doca_ec_matrix_type MATRIX_TYPE{DOCA_EC_MATRIX_TYPE_CAUCHY};

std::string dev_pci_addr;
std::string output_file;
size_t block_size;
size_t data_block_count;
size_t rdnc_block_count;
size_t max_create_task_num;
size_t max_recover_task_num;

size_t repeat_time;
size_t total_block_count;

void read_args(int argc, char **argv) {
    argparse::ArgumentParser program("ec_bench");
    
    program.add_argument("-a")
        .help("device pci address")
        .required();
    program.add_argument("-o")
        .help("output file")
        .required();
    program.add_argument("-b")
        .help("block size")
        .required()
        .scan<'d', size_t>();
    program.add_argument("-d")
        .help("data block num")
        .required()
        .scan<'d', size_t>();
    program.add_argument("-r")
        .help("redundant block num")
        .required()
        .scan<'d', size_t>();
    program.add_argument("-n")
        .help("max create task num")
        .required()
        .scan<'d', size_t>();
    program.add_argument("-m")
        .help("max recover task num")
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
    output_file = program.get<std::string>("-o");
    block_size = program.get<size_t>("-b");
    data_block_count = program.get<size_t>("-d");
    rdnc_block_count = program.get<size_t>("-r");
    max_create_task_num = program.get<size_t>("-n");
    max_recover_task_num = program.get<size_t>("-m");

    total_block_count = data_block_count + rdnc_block_count;

    if (max_create_task_num >= 10) {
        repeat_time = 1;
    } else {
        repeat_time = 10 / max_create_task_num + 1;
    }
    
    if (max_recover_task_num >= 10) {
        repeat_time = 1;
    } else {
        repeat_time = 10 / max_recover_task_num + 1;
    } 

    std::cout << "Device pci address: " << dev_pci_addr << std::endl;
    std::cout << "Block size: " << block_size << std::endl;
    std::cout << "Output file: " << output_file << std::endl;
    std::cout << "Data block num: " << data_block_count << std::endl;
    std::cout << "Redundant block num: " << rdnc_block_count << std::endl;
}

int main(int argc, char **argv) {
    read_args(argc, argv);
    
    // open device
    doca_dev *dev{};
    docacxx::check_derr(docacxx::open_doca_device_with_pci(dev_pci_addr.data(), &dev),
            "failed to open dev");

    // create ec encoder
    ErasureCoder encoder(dev, MATRIX_TYPE, data_block_count, rdnc_block_count,
        max_create_task_num, max_recover_task_num);

    // ec create bench
    docacxx::BufInventory src_buf_inv(dev, max_create_task_num * (data_block_count * block_size),
        max_create_task_num);
    docacxx::BufInventory dst_buf_inv(dev, max_create_task_num * (rdnc_block_count * block_size),
        max_create_task_num);
    for (size_t i{}; i < src_buf_inv.len; ++i) {
        src_buf_inv.mem[i] = random();
    }

    std::vector<std::unique_ptr<docacxx::Buffer>> src_bufs;
    src_bufs.reserve(max_create_task_num);
    for (size_t i{}; i < max_create_task_num; ++i) {
        auto buf_len = data_block_count * block_size;
        src_bufs.emplace_back(src_buf_inv.get_buf(i * buf_len, buf_len));
        src_bufs.back()->set_doca_buf_data(buf_len);
    }

    std::vector<std::unique_ptr<docacxx::Buffer>> dst_bufs;
    dst_bufs.reserve(max_create_task_num);
    for (size_t i{}; i < max_create_task_num; ++i) {
        auto buf_len = rdnc_block_count * block_size;
        dst_bufs.emplace_back(dst_buf_inv.get_buf(i * buf_len, buf_len));
    }

    auto start = Timer::now();
    encoder.create(src_bufs, dst_bufs);

    for (size_t i{1}; i < repeat_time; ++i) {
        for (auto &dst_buf : dst_bufs) {
            dst_buf->clear_doca_buf_data();
        }
        encoder.create(src_bufs, dst_bufs);
    }

    auto end = Timer::now();
    auto create_time = duration_cast<Duration>(end - start);

    // ec recover bench
    size_t intact_block_count = data_block_count - rdnc_block_count;
    std::vector<uint32_t> missing_indices;
    for (uint32_t i{}; i < rdnc_block_count; ++i) {
        missing_indices.emplace_back(i + intact_block_count);
    }

    docacxx::BufInventory available_buf_inv(dev,
        max_recover_task_num * (data_block_count * block_size), max_recover_task_num);
    docacxx::BufInventory recovered_buf_inv(dev,
        max_recover_task_num * (rdnc_block_count * block_size), max_recover_task_num);
    
    std::vector<std::unique_ptr<docacxx::Buffer>> available_bufs;
    available_bufs.reserve(max_recover_task_num);
    for (size_t i{}; i < max_recover_task_num; ++i) {
        size_t buf_len = data_block_count * block_size;
        available_bufs.emplace_back(available_buf_inv.get_buf(i * buf_len, buf_len));
        auto &buf = available_bufs.back();
        
        std::memcpy(buf->mem, src_bufs[i]->mem, intact_block_count * block_size);
        std::memcpy(buf->mem + intact_block_count * block_size,
            dst_bufs[i]->mem, rdnc_block_count * block_size);
        buf->set_doca_buf_data(buf_len);
    }

    std::vector<std::unique_ptr<docacxx::Buffer>> recovered_bufs;
    recovered_bufs.reserve(max_recover_task_num);
    for (size_t i{}; i < max_recover_task_num; ++i) {
        size_t buf_len = rdnc_block_count * block_size;
        recovered_bufs.emplace_back(recovered_buf_inv.get_buf(i * buf_len, buf_len));
    }

    start = Timer::now();
    encoder.recover(available_bufs, recovered_bufs, missing_indices);
    for (size_t i{1}; i < repeat_time; ++i) {
        for (auto &recovered_buf : recovered_bufs) {
            recovered_buf->clear_doca_buf_data();
        }
        encoder.recover(available_bufs, recovered_bufs, missing_indices);
    }

    end = Timer::now();
    auto recover_time = duration_cast<Duration>(end - start);

    // test recover
    for (size_t i{}; i < max_recover_task_num; ++i) {
        std::string_view a = recovered_bufs[i]->get_doca_buf_data();
        std::string_view b(src_bufs[i]->mem + intact_block_count * block_size,
            rdnc_block_count * block_size);
        if (a != b) {
            std::cerr << "Failed to recover" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    // output_perf
    std::ofstream output_fs{output_file};
    if (not output_fs) {
        std::cerr << "Unable to open " << output_file << std::endl;
        exit(EXIT_FAILURE);   
    }

    Json json;
    json["data_block_count"] = data_block_count;
    json["rdnc_block_count"] = rdnc_block_count;
    json["block_size"] = block_size;
    
    json["create"]["task_num"] = max_create_task_num;
    json["create"]["count"] = max_create_task_num * repeat_time;
    json["create"]["time"] = duration_str(create_time);

    json["recover"]["task_num"] = max_recover_task_num;
    json["recover"]["count"] = max_recover_task_num * repeat_time;
    json["recover"]["time"] = duration_str(recover_time);

    std::cout << json.dump(4) << std::endl;
    output_fs << json.dump(4) << std::endl;
    output_fs.close();

    return 0;
}
