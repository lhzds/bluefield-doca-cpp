#include <cstdlib>
#include <doca_ctx.h>
#include <doca_error.h>
#include <doca_pe.h>
#include <iostream>
#include <memory>
#include <lz4.h>
#include <isa-l.h>

#include "docacxx/ctx/compress.h"
#include "docacxx/doca_utils.h"
#include "docacxx/mem.h"
#include "docacxx/task_context.hpp"

class Compressor {
public:
    virtual ~Compressor() {
        m_decompressor->clean_sync();

        auto result = doca_pe_destroy(m_pe);
        if (result != DOCA_SUCCESS) {
            std::cerr << "Failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    virtual size_t compress_bound(size_t sz) = 0;
    virtual size_t compress(const char *src, char *dst, size_t src_len, size_t dst_len) = 0;
    virtual void decompress(const std::vector<std::unique_ptr<docacxx::Buffer>> &src,
        std::vector<std::unique_ptr<docacxx::Buffer>> &dst) = 0;

protected:
    doca_pe *m_pe{};
    std::unique_ptr<docacxx::Compress> m_decompressor;
};

class Lz4Compressor: public Compressor {
public:
    Lz4Compressor(doca_dev *dev, size_t decompress_task_slot_num)
        :m_decompress_ctx(decompress_task_slot_num) {
        docacxx::check_derr(doca_pe_create(&m_pe), "Failed to create pe");

        docacxx::Compress::Cfg cfg;
        cfg.m_lz4_block_decompress_task_slot_num = decompress_task_slot_num;

        m_decompressor = std::make_unique<docacxx::Compress>(m_pe, dev, cfg);
        m_decompressor->set_lz4_block_decompress_cb(m_decompress_ctx.make_pipeline_submitter());
        m_decompressor->start_sync();
    }

    size_t compress_bound(size_t sz) override {
        return LZ4_compressBound(sz);
    }

    size_t compress(const char *src, char *dst, size_t src_len, size_t dst_len) override {
        return LZ4_compress_default(src, dst, src_len, dst_len);
    }

    void decompress(const std::vector<std::unique_ptr<docacxx::Buffer>> &src,
        std::vector<std::unique_ptr<docacxx::Buffer>> &dst) override {
        m_decompress_ctx.reset();
        assert(src.size() == dst.size());

        for (size_t i{}; i < src.size(); ++i) {
            m_decompress_ctx.submit([&, i] {
                auto task = m_decompressor->alloc_lz4_block_decompress_task(*src[i], *dst[i], nullptr);
                m_decompressor->submit(std::move(task));
            });
        }
        while (m_decompress_ctx.complete_cnt() != src.size()) {
            doca_pe_progress(m_pe);
        }
    }

private:
    docacxx::TaskContext<docacxx::Compress::Lz4BlockDecompressTask> m_decompress_ctx;
};

class DeflateCompressor: public Compressor {
public:
    DeflateCompressor(doca_dev *dev, size_t decompress_task_slot_num)
        :m_decompress_ctx(decompress_task_slot_num) {
        docacxx::check_derr(doca_pe_create(&m_pe), "Failed to create pe");

        docacxx::Compress::Cfg cfg;
        cfg.m_deflate_decompress_task_slot_num = decompress_task_slot_num;

        m_decompressor = std::make_unique<docacxx::Compress>(m_pe, dev, cfg);
        m_decompressor->set_deflate_decompress_cb(m_decompress_ctx.make_pipeline_submitter());
        m_decompressor->start_sync();
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

    void decompress(const std::vector<std::unique_ptr<docacxx::Buffer>> &src,
        std::vector<std::unique_ptr<docacxx::Buffer>> &dst) override {
        m_decompress_ctx.reset();
        assert(src.size() == dst.size());

        for (size_t i{}; i < src.size(); ++i) {
            m_decompress_ctx.submit([&, i] {
                auto task = m_decompressor->alloc_deflate_decompress_task(*src[i], *dst[i], nullptr);
                m_decompressor->submit(std::move(task));
            });
        }
        while (m_decompress_ctx.complete_cnt() != src.size()) {
            doca_pe_progress(m_pe);
        }
    }

private:
    docacxx::TaskContext<docacxx::Compress::DeflateDecompressTask> m_decompress_ctx;
};
