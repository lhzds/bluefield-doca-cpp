#include <argparse/argparse.hpp>
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <fstream>

#include "docacxx/mem.h"
#include "clock.h"
#include "ec_coder.h"
#include "scoped_guard.h"
#include "print_util.h"

using namespace std::chrono;
using Duration = std::chrono::microseconds;
using Json = nlohmann::json;

constexpr doca_ec_matrix_type MATRIX_TYPE{DOCA_EC_MATRIX_TYPE_CAUCHY};

std::string dev_pci_addr;
std::string output_file;
size_t block_size;
size_t data_block_count;
size_t rdnc_block_count;
size_t create_task_slot_num;
size_t recover_task_slot_num;

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
        .help("create task slot num")
        .required()
        .scan<'d', size_t>();
    program.add_argument("-m")
        .help("recover task slot num")
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
    create_task_slot_num = program.get<size_t>("-n");
    recover_task_slot_num = program.get<size_t>("-m");

    total_block_count = data_block_count + rdnc_block_count;

    if (create_task_slot_num >= 10) {
        repeat_time = 1;
    } else {
        repeat_time = 10 / create_task_slot_num + 1;
    }
    
    if (recover_task_slot_num >= 10) {
        repeat_time = 1;
    } else {
        repeat_time = 10 / recover_task_slot_num + 1;
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
            "Failed to open dev");
    ScopeGuard guard([&] {
        docacxx::check_derr(doca_dev_close(dev), "Failed to close dev");
    });

    // create ec encoder
    EcCoder coder(dev, MATRIX_TYPE, data_block_count, rdnc_block_count,
        create_task_slot_num, recover_task_slot_num);

    // ec create bench
    docacxx::BufInventoryWithMmap src_buf_inv(dev, create_task_slot_num * (data_block_count * block_size),
        create_task_slot_num);
    docacxx::BufInventoryWithMmap dst_buf_inv(dev, create_task_slot_num * (rdnc_block_count * block_size),
        create_task_slot_num);
    for (size_t i{}; i < src_buf_inv.size(); ++i) {
        src_buf_inv.data()[i] = random();
    }

    std::vector<std::unique_ptr<docacxx::Buffer>> src_bufs;
    src_bufs.reserve(create_task_slot_num);
    for (size_t i{}; i < create_task_slot_num; ++i) {
        auto buf_len = data_block_count * block_size;
        src_bufs.emplace_back(src_buf_inv.get_buf(i * buf_len, buf_len));
        src_bufs.back()->set_doca_buf_data(buf_len);
    }

    std::vector<std::unique_ptr<docacxx::Buffer>> dst_bufs;
    dst_bufs.reserve(create_task_slot_num);
    for (size_t i{}; i < create_task_slot_num; ++i) {
        auto buf_len = rdnc_block_count * block_size;
        dst_bufs.emplace_back(dst_buf_inv.get_buf(i * buf_len, buf_len));
    }

    auto create_time = clock([&] {
        coder.create(src_bufs, dst_bufs);
    });
    for (size_t i{1}; i < repeat_time; ++i) {
        for (auto &dst_buf : dst_bufs) {
            dst_buf->clear_doca_buf_data();
        }
        create_time += clock([&] {
            coder.create(src_bufs, dst_bufs);
        });
    }

    // ec recover bench
    std::set<uint32_t> missing_set;
    while (missing_set.size() < rdnc_block_count) {
        missing_set.emplace(random() % data_block_count);
    }
    std::vector<uint32_t> missing_indices(missing_set.begin(), missing_set.end());

    std::cout << "Missing indices: ";
    for (auto index : missing_set) {
        std::cout << index << " ";
    }
    std::cout << std::endl;

    docacxx::BufInventoryWithMmap avail_buf_inv(dev,
        recover_task_slot_num * (data_block_count * block_size), recover_task_slot_num);
    docacxx::BufInventoryWithMmap recovered_buf_inv(dev,
        recover_task_slot_num * (rdnc_block_count * block_size), recover_task_slot_num);
    
    std::vector<std::unique_ptr<docacxx::Buffer>> avail_bufs;
    avail_bufs.reserve(recover_task_slot_num);
    std::vector<std::vector<char>> test_data;
    test_data.reserve(recover_task_slot_num);

    for (size_t i{}; i < recover_task_slot_num; ++i) {
        test_data.emplace_back(std::vector<char>(rdnc_block_count * block_size));

        size_t avail_buf_len = data_block_count * block_size;
        avail_bufs.emplace_back(avail_buf_inv.get_buf(i * avail_buf_len, avail_buf_len));
        avail_bufs[i]->set_doca_buf_data(avail_buf_len);
        
        size_t test_blocks{}, avail_blocks{};
        for (size_t j{}; j < total_block_count; ++j) {
            if (missing_set.count(j)) {
                std::memcpy(
                    test_data[i].data() + test_blocks * block_size,
                    src_bufs[i]->data() + j * block_size,
                    block_size
                );
                ++test_blocks;         
            } else {
                std::memcpy(
                    avail_bufs[i]->data() + avail_blocks * block_size,
                    src_bufs[i]->data() + j * block_size,
                    block_size
                );
                ++avail_blocks;
            }
        }
    }

    std::vector<std::unique_ptr<docacxx::Buffer>> recovered_bufs;
    recovered_bufs.reserve(recover_task_slot_num);
    for (size_t i{}; i < recover_task_slot_num; ++i) {
        size_t buf_len = rdnc_block_count * block_size;
        recovered_bufs.emplace_back(recovered_buf_inv.get_buf(i * buf_len, buf_len));
    }

    auto recover_time = clock([&] {
        coder.recover(avail_bufs, recovered_bufs, missing_indices);
    });
    for (size_t i{1}; i < repeat_time; ++i) {
        for (auto &recovered_buf : recovered_bufs) {
            recovered_buf->clear_doca_buf_data();
        }
        recover_time += clock([&] {
        coder.recover(avail_bufs, recovered_bufs, missing_indices);
    });
    }

    // test recover
    for (size_t i{}; i < recover_task_slot_num; ++i) {
        auto doca_buf_data = recovered_bufs[i]->get_doca_buf_data();
        std::string_view a(doca_buf_data.first, doca_buf_data.second);
        std::string_view b(test_data[i].data(), test_data[i].size());
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
    
    json["create"]["task_slot_num"] = create_task_slot_num;
    json["create"]["count"] = create_task_slot_num * repeat_time;
    json["create"]["time"] = duration_str(duration_cast<Duration>(create_time));

    json["recover"]["task_slot_num"] = recover_task_slot_num;
    json["recover"]["count"] = recover_task_slot_num * repeat_time;
    json["recover"]["time"] = duration_str(duration_cast<Duration>(recover_time));

    std::cout << json.dump(4) << std::endl;
    output_fs << json.dump(4) << std::endl;
    output_fs.close();
    return 0;
}
