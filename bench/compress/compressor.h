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
#include <isa-l.h>

class Compressor {
public:
    Compressor(size_t doca_max_decompress_task_num)
        :doca_max_decompress_task_num_{doca_max_decompress_task_num} {}

    void init() {
        decompressor_->custom_decompress_cb = [&](doca_data task_user_data) {
            if (not wait_queue_.empty()) {
                auto [src_buf, dst_buf] = wait_queue_.front();
                wait_queue_.pop();
                auto *task = decompressor_->alloc_decompress_task(src_buf, dst_buf);
                decompressor_->submit(task);
            }

            ++finished_;
        };
        decompressor_->start();
        while (decompressor_->state != DOCA_CTX_STATE_RUNNING) {
            doca_pe_progress(pe_);
        }
    }

    virtual ~Compressor() {
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

    virtual size_t compress_bound(size_t sz) = 0;
    virtual size_t compress(const char *src, char *dst, size_t src_len, size_t dst_len) = 0;

    void decompress(const std::vector<std::unique_ptr<docacxx::Buffer>> &src,
        std::vector<std::unique_ptr<docacxx::Buffer>> &dst) {
        finished_ = 0;
        assert(src.size() == dst.size());

        for (size_t i{}; i < std::min(src.size(), doca_max_decompress_task_num_); ++i) {
            auto *task = decompressor_->alloc_decompress_task(src[i]->buf, dst[i]->buf);
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

protected:
    doca_pe *pe_{};
    size_t doca_max_decompress_task_num_{};
    std::unique_ptr<docacxx::BufInventory> buf_inv_;
    std::unique_ptr<docacxx::Compress> decompressor_;
    size_t finished_{};
    std::queue<std::pair<const doca_buf *, doca_buf *>> wait_queue_;
};

class Lz4Compressor: public Compressor {
public:
    Lz4Compressor(doca_dev *dev, size_t doca_max_decompress_task_num)
        :Compressor(doca_max_decompress_task_num) {
        docacxx::check_derr(doca_pe_create(&pe_), "failed to create pe");

        docacxx::Compress::Cfg cfg;
        cfg.decompress_task_num = doca_max_decompress_task_num;

        decompressor_ = std::make_unique<docacxx::Lz4Compress>(pe_, dev, cfg);
        init();
    }

    size_t compress_bound(size_t sz) override {
        return LZ4_compressBound(sz);
    }

    size_t compress(const char *src, char *dst, size_t src_len, size_t dst_len) override {
        return LZ4_compress_default(src, dst, src_len, dst_len);
    }
};

class DeflateCompressor: public Compressor {
public:
    DeflateCompressor(doca_dev *dev, size_t doca_max_decompress_task_num)
        :Compressor(doca_max_decompress_task_num) {
        docacxx::check_derr(doca_pe_create(&pe_), "failed to create pe");

        docacxx::Compress::Cfg cfg;
        cfg.decompress_task_num = doca_max_decompress_task_num;

        decompressor_ = std::make_unique<docacxx::DeflateCompress>(pe_, dev, cfg);
        init();
    }

    size_t compress_bound(size_t sz) override {
        return sz + ISAL_DEF_MAX_HDR_SIZE;
    }

    size_t compress(const char *src, char *dst, size_t src_len, size_t dst_len) override {
        isal_zstream stream{};
        isal_deflate_stateless_init(&stream);
        stream.next_in = (uint8_t *)(src);
        stream.avail_in = src_len;
        stream.next_out = (uint8_t *)(dst);
        stream.avail_out = dst_len;
        // stream.end_of_stream = true;

        auto err = isal_deflate_stateless(&stream);
        if (err != COMP_OK) {
            std::cerr << "Failed to compress with deflate" << std::endl;
            exit(EXIT_FAILURE);
        }

        return stream.total_out;
    }
};