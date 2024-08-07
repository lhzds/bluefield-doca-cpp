#include "compressor.h"
#include "docacxx/mem.h"
#include "../part_iterator.h"
#include <argparse/argparse.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using namespace std::chrono;
using Timer = std::chrono::high_resolution_clock;
using Duration = std::chrono::microseconds;

using Json = nlohmann::json;

std::string pci_addr;
size_t block_size;
std::string input_file;
std::string output_file;
constexpr size_t MAX_DECOMPRESS_TASK_NUM{65536};

void read_args(int argc, char **argv) {
    argparse::ArgumentParser program("compress_bench");
    
    program.add_argument("-a")
        .help("device pci address")
        .required();
    program.add_argument("-i")
        .help("input file")
        .required();
    program.add_argument("-o")
        .help("output file")
        .required();
    program.add_argument("-b")
        .help("block size")
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

    pci_addr = program.get<std::string>("-a");
    block_size = program.get<size_t>("-b");
    input_file = program.get<std::string>("-i");
    output_file = program.get<std::string>("-o");

    std::cout << "Device pci address: " << pci_addr << std::endl;
    std::cout << "Block size: " << block_size << std::endl;
    std::cout << "Input file: " << input_file << std::endl;
    std::cout << "Output file: " << output_file << std::endl;
}

std::vector<std::unique_ptr<docacxx::Buffer>> compress(const std::string &in_data,
    docacxx::BufInventory &buf_inv, Lz4Compressor &compressor) {
    std::vector<std::unique_ptr<docacxx::Buffer>> compresseds;
    PartIterator in_part_iter(in_data.size(), block_size);
    PartIterator out_part_iter(buf_inv.len, compressor.compress_bound(block_size));

    while (in_part_iter.has_next() && out_part_iter.has_next()) {
        auto [in_offset, in_len] = in_part_iter.next();
        auto [out_offset, out_len] = out_part_iter.next();

        compresseds.emplace_back(buf_inv.get_buf(out_offset, out_len));
        auto &buf = compresseds.back();

        auto clen = compressor.compress(in_data.data() + in_offset, buf->mem,
            in_len, buf->len);
        buf->set_doca_buf_data(clen);
    }

    if (in_part_iter.has_next() || out_part_iter.has_next()) {
        throw std::runtime_error("error allocate data");
    }
    
    return compresseds;
}

int main(int argc, char **argv) {
    read_args(argc, argv);

    // open input file
    std::ifstream input_fs{input_file, std::ios::binary};
    if (not input_fs) {
        std::cerr << "Unable to open " << input_file << std::endl;
        exit(EXIT_FAILURE);
    }

    // get file size
    input_fs.seekg(0, std::ios::end);
    std::streamsize file_size = input_fs.tellg();
    input_fs.seekg(0, std::ios::beg);
    std::cout << "File size: " << file_size << std::endl;

    // read content of file into string
    std::string file_data(file_size, '\0'); // 预分配字符串大小
    if (!input_fs.read(&file_data[0], file_size)) {
        std::cerr << "Error reading file " << input_file << std::endl;
        return EXIT_FAILURE;
    }
    
    // open device
    doca_dev *dev{};
    docacxx::check_derr(docacxx::open_doca_device_with_pci(pci_addr.data(), &dev),
            "failed to open dev");

    // create compressor
    Lz4Compressor lz4_compressor(dev, MAX_DECOMPRESS_TASK_NUM);

    // create doca buf inv
    size_t block_num = (file_size + block_size - 1) / block_size;
    docacxx::BufInventory src_buf_inv(dev, block_num * lz4_compressor.compress_bound(block_size),
        block_num);
    docacxx::BufInventory dst_buf_inv(dev, block_num * block_size, block_num);

    // create lz4-compressed source data
    std::vector<std::unique_ptr<docacxx::Buffer>> compresseds = compress(file_data, src_buf_inv, lz4_compressor);

    // decompress_bench (cuz bf3 only supports lz4 decompression)
    std::vector<std::unique_ptr<docacxx::Buffer>> decompresseds;
    PartIterator part_iter(file_size, block_size);
    while (part_iter.has_next()) {
        auto [offset, len] = part_iter.next();
        decompresseds.emplace_back(dst_buf_inv.get_buf(offset, len));
    }

    auto start = Timer::now();
    lz4_compressor.decompress(compresseds, decompresseds);
    auto end = Timer::now();
    auto decompress_time = duration_cast<Duration>(end - start);

    // test
    std::string_view result(dst_buf_inv.mem, file_size);

    if (file_data != result) {
        std::cerr << "the contents before and after compression are different" << std::endl;
        exit(EXIT_FAILURE);
    }

    // output_perf
    std::ofstream output_fs{output_file};
    if (not output_fs) {
        std::cerr << "Unable to open " << output_file << std::endl;
        exit(EXIT_FAILURE);   
    }

    Json json;
    json["block_size"] = block_size;
    json["file_size"] = file_size;
    json["decompress_time"] = fmt::format("{:.2f}", static_cast<double>(decompress_time.count()));

    std::cout << json.dump(4) << std::endl;
    output_fs << json.dump(4) << std::endl;
    output_fs.close();

    return 0;
}