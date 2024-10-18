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
        :m_doca_max_decompress_task_num{doca_max_decompress_task_num} {}

    void init() {
        m_decompressor->custom_decompress_cb = [&](doca_data task_user_data) {
            if (not m_wait_queue.empty()) {
                auto [src_buf, dst_buf] = m_wait_queue.front();
                m_wait_queue.pop();
                auto *task = m_decompressor->alloc_decompress_task(src_buf, dst_buf);
                m_decompressor->submit(task);
            }

            ++m_finished;
        };
        m_decompressor->start_sync();
    }

    virtual ~Compressor() {
        m_decompressor->clean_sync();

        auto result = doca_pe_destroy(m_pe);
        if (result != DOCA_SUCCESS) {
            std::cerr << "failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    virtual size_t compress_bound(size_t sz) = 0;
    virtual size_t compress(const char *src, char *dst, size_t src_len, size_t dst_len) = 0;

    void decompress(const std::vector<std::unique_ptr<docacxx::Buffer>> &src,
        std::vector<std::unique_ptr<docacxx::Buffer>> &dst) {
        m_finished = 0;
        assert(src.size() == dst.size());

        for (size_t i{}; i < std::min(src.size(), m_doca_max_decompress_task_num); ++i) {
            auto *task = m_decompressor->alloc_decompress_task(src[i]->m_inner, dst[i]->m_inner);
            m_decompressor->submit(task);
        }

        if (src.size() > m_doca_max_decompress_task_num) {
            for (size_t i = m_doca_max_decompress_task_num; i < src.size(); ++i) {
                m_wait_queue.emplace(src[i]->m_inner, dst[i]->m_inner);
            }
        }
        while (m_finished != src.size()) {
            doca_pe_progress(m_pe);
        }
    }

protected:
    doca_pe *m_pe{};
    size_t m_doca_max_decompress_task_num{};
    std::unique_ptr<docacxx::Compress> m_decompressor;
    size_t m_finished{};
    std::queue<std::pair<const doca_buf *, doca_buf *>> m_wait_queue;
};

class Lz4Compressor: public Compressor {
public:
    Lz4Compressor(doca_dev *dev, size_t doca_max_decompress_task_num)
        :Compressor(doca_max_decompress_task_num) {
        docacxx::check_derr(doca_pe_create(&m_pe), "failed to create pe");

        docacxx::Compress::Cfg cfg;
        cfg.m_decompress_task_num = doca_max_decompress_task_num;

        m_decompressor = std::make_unique<docacxx::Lz4Compress>(m_pe, dev, cfg);
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
        docacxx::check_derr(doca_pe_create(&m_pe), "failed to create pe");

        docacxx::Compress::Cfg cfg;
        cfg.m_decompress_task_num = doca_max_decompress_task_num;

        m_decompressor = std::make_unique<docacxx::DeflateCompress>(m_pe, dev, cfg);
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
