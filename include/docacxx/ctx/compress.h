#pragma once

#include "docacxx/context.h"
#include "docacxx/mem.h"
#include <doca_compress.h>
#include <doca_pe.h>
#include <functional>
#include <stdexcept>

namespace docacxx {
class Compress: public Context {
public:
    using CompleteCallback = std::function<void()>;

    struct Cfg {
        size_t m_decompress_task_slot_num{};
    };

    Compress(doca_pe *pe, doca_dev *dev, const Cfg &cfg);
    ~Compress() {};

    doca_compress *raw() noexcept {
        return m_inner;
    }
    const doca_compress *raw() const noexcept {
        return m_inner;
    }

    virtual std::unique_ptr<Task> alloc_decompress_task(const Buffer &src, Buffer &dst,
        Task::CompleteCallback cb) = 0;
        
    void set_decompress_cb(CompleteCallback cb) {
        m_decompress_cb = std::move(cb);
    }
    void set_decompress_error_cb(CompleteCallback cb) {
        m_decompress_error_cb = std::move(cb);
    }

protected:
    void clean_impl() override;
    
    doca_compress *m_inner{};
    CompleteCallback m_decompress_cb{[]() {}};
    CompleteCallback m_decompress_error_cb{[]() {
        throw std::runtime_error("decompress error.");
    }};
};

class Lz4Compress: public Compress {
public:
    Lz4Compress(doca_pe *pe, doca_dev *dev, const Cfg &cfg);
    ~Lz4Compress() = default;

    std::unique_ptr<Task> alloc_decompress_task(const Buffer &src, Buffer &dst,
        Task::CompleteCallback cb) override;

private:
    static void decompress_lz4_block_cb(
        doca_compress_task_decompress_lz4_block *raw_decompress_lz4_block_task,
        doca_data task_data,
        doca_data ctx_data);
    static void decompress_lz4_block_error_cb(
        doca_compress_task_decompress_lz4_block *raw_decompress_lz4_block_task,
        doca_data task_data,
        doca_data ctx_data);
};

class DeflateCompress: public Compress {
public:
    DeflateCompress(doca_pe *pe, doca_dev *dev, const Cfg &cfg);
    ~DeflateCompress() = default;

    std::unique_ptr<Task> alloc_decompress_task(const Buffer &src, Buffer &dst,
        Task::CompleteCallback cb) override;

private:
    static void decompress_deflate_cb(
        doca_compress_task_decompress_deflate *raw_decompress_deflate_task,
        doca_data task_data,
        doca_data ctx_data);
    static void decompress_deflate_error_cb(
        doca_compress_task_decompress_deflate *raw_decompress_deflate_task,
        doca_data task_data,
        doca_data ctx_data);
};
}
