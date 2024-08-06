#include "docacxx/compress.h"
#include "docacxx/doca_utils.h"
#include "docacxx/mem.h"
#include <cstdlib>
#include <doca_ctx.h>
#include <doca_error.h>
#include <doca_pe.h>
#include <iostream>
#include <memory>
#include <lz4.h>

class Lz4Compressor {
public:
    Lz4Compressor(doca_dev *dev, size_t doca_max_decompress_task_num) {
        docacxx::check_derr(doca_pe_create(&pe_), "failed to create pe");

        docacxx::Compress::Cfg cfg;
        cfg.decompress_lz4_block_task_num = doca_max_decompress_task_num;

        decompressor_ = std::make_unique<docacxx::Compress>(pe_, dev, cfg);
        decompressor_->custom_decompress_lz4_block_cb = [&](doca_data task_user_data) {
            ++finished_;
        };
        decompressor_->start();
        while (decompressor_->state != DOCA_CTX_STATE_RUNNING) {
            doca_pe_progress(pe_);
        }
    }

    ~Lz4Compressor() {
        decompressor_->stop();
        while (decompressor_->state != DOCA_CTX_STATE_IDLE) {
            doca_pe_progress(pe_);
        }
        decompressor_->clean();

        auto result = doca_pe_destroy(pe_);
        if (result != DOCA_SUCCESS) {
            std::cerr << "failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    size_t compress(const char *src, char *dst, size_t src_len, size_t dst_len) {
        return LZ4_compress_default(src, dst, src_len, dst_len);
    }

    void decompress(const std::vector<std::pair<const doca_buf *, doca_buf *>> &requests) {
        assert(requests.size() <= doca_max_decompress_task_num);
        finished_ = 0;
        for (auto &[src, dst] : requests) {
            auto *task = decompressor_->alloc_decompress_lz4_block_task(src, dst);
            decompressor_->submit(task);
        }
        while (finished_ != requests.size()) {
            doca_pe_progress(pe_);
        }
    }

private:
    doca_pe *pe_{};
    size_t doca_max_decompress_task_num{};
    std::unique_ptr<docacxx::BufInventory> buf_inv_;
    std::unique_ptr<docacxx::Compress> decompressor_;
    size_t finished_{};
};

int main() {
    std::string pci_addr = "03:00.1";
    doca_dev *dev;
    docacxx::check_derr(docacxx::open_doca_device_with_pci(pci_addr.data(), &dev),
            "failed to open dev");
    
    std::string src(1024, 'a');
    Lz4Compressor lz4_compressor(dev, 1);

    docacxx::BufInventory buf_inv(dev, 2048, 2);
    auto compressed = buf_inv.get_buf(0, 1024);
    auto decompressed = buf_inv.get_buf(1024, 1024);

    auto clen = lz4_compressor.compress(src.data(), compressed.mem,
        src.length(), compressed.len);

    compressed.set_doca_buf_data(clen);
    lz4_compressor.decompress({{compressed.buf, decompressed.buf}});


    if (src != decompressed.get_doca_buf_data()) {
        std::cerr << "The contents before and after compression are inconsistent" << std::endl;
        exit(EXIT_FAILURE);
    }

    return 0;
}
