#include "docacxx/compress.h"
#include "docacxx/doca_utils.h"
#include "docacxx/mem.h"
#include <cstdlib>
#include <doca_ctx.h>
#include <doca_error.h>
#include <doca_pe.h>
#include <iostream>
#include <queue>
#include <memory>
#include <lz4.h>

class Lz4Compressor {
public:
    Lz4Compressor(doca_dev *dev, size_t doca_max_decompress_task_num)
        :doca_max_decompress_task_num_{doca_max_decompress_task_num} {
        docacxx::check_derr(doca_pe_create(&pe_), "failed to create pe");

        docacxx::Compress::Cfg cfg;
        cfg.decompress_lz4_block_task_num = doca_max_decompress_task_num;

        decompressor_ = std::make_unique<docacxx::Compress>(pe_, dev, cfg);
        decompressor_->custom_decompress_lz4_block_cb = [&](doca_data task_user_data) {
            if (not wait_queue_.empty()) {
                auto [src_buf, dst_buf] = wait_queue_.front();
                wait_queue_.pop();
                auto *task = decompressor_->alloc_decompress_lz4_block_task(src_buf, dst_buf);
                decompressor_->submit(task);
            }

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

    size_t compress_bound(size_t sz) {
        return LZ4_compressBound(sz);
    }

    size_t compress(const char *src, char *dst, size_t src_len, size_t dst_len) {
        return LZ4_compress_default(src, dst, src_len, dst_len);
    }

    void decompress(const std::vector<std::unique_ptr<docacxx::Buffer>> &src,
        std::vector<std::unique_ptr<docacxx::Buffer>> &dst) {
        finished_ = 0;
        assert(src.size() == dst.size());

        for (size_t i{}; i < std::min(src.size(), doca_max_decompress_task_num_); ++i) {
            auto *task = decompressor_->alloc_decompress_lz4_block_task(src[i]->buf, dst[i]->buf);
            decompressor_->submit(task);
        }

        if (src.size() > doca_max_decompress_task_num_) {
            for (size_t i = doca_max_decompress_task_num_; i < src.size(); ++i) {
                wait_queue_.emplace(src[i]->buf, dst[i]->buf);
            }
        }
        while (finished_ != src.size()) {
            doca_pe_progress(pe_);
        }
    }

private:
    doca_pe *pe_{};
    size_t doca_max_decompress_task_num_{};
    std::unique_ptr<docacxx::BufInventory> buf_inv_;
    std::unique_ptr<docacxx::Compress> decompressor_;
    size_t finished_{};
    std::queue<std::pair<const doca_buf *, doca_buf *>> wait_queue_;
};
