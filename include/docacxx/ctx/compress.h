#pragma once

#include <doca_compress.h>
#include <doca_pe.h>
#include <functional>
#include <stdexcept>

#include "docacxx/context.h"
#include "docacxx/mem.h"
#include "docacxx/task.hpp"

namespace docacxx {
class Compress: public Context {
public:
    using Lz4BlockDecompressTask = Task<doca_compress_task_decompress_lz4_block>;
    using DeflateDecompressTask = Task<doca_compress_task_decompress_deflate>;

    struct Cfg {
        size_t m_lz4_block_decompress_task_slot_num{};
        size_t m_deflate_decompress_task_slot_num{};
    };

    Compress(doca_pe *pe, doca_dev *dev, const Cfg &cfg);
    ~Compress() {};

    doca_compress *raw() noexcept {
        return m_inner;
    }
    const doca_compress *raw() const noexcept {
        return m_inner;
    }

    std::unique_ptr<Lz4BlockDecompressTask> alloc_lz4_block_decompress_task(const Buffer &src, Buffer &dst,
        Lz4BlockDecompressTask::CompleteCallback cb);
    std::unique_ptr<Lz4BlockDecompressTask> submit(std::unique_ptr<Lz4BlockDecompressTask>);
    void set_lz4_block_decompress_cb(Lz4BlockDecompressTask::ContextCompleteCallback cb) {
        m_lz4_block_decompress_cb = std::move(cb);
    }
    void set_lz4_block_decompress_error_cb(Lz4BlockDecompressTask::ContextCompleteCallback cb) {
        m_lz4_block_decompress_cb = std::move(cb);
    }

    std::unique_ptr<DeflateDecompressTask> alloc_deflate_decompress_task(const Buffer &src, Buffer &dst,
        DeflateDecompressTask::CompleteCallback cb);
    std::unique_ptr<DeflateDecompressTask> submit(std::unique_ptr<DeflateDecompressTask>);
    void set_deflate_decompress_cb(DeflateDecompressTask::ContextCompleteCallback cb) {
        m_deflate_decompress_cb = std::move(cb);
    }
    void set_deflate_decompress_error_cb(DeflateDecompressTask::ContextCompleteCallback cb) {
        m_deflate_decompress_error_cb = std::move(cb);
    }

protected:
    void clean_impl() override;

    static void decompress_lz4_block_cb(
        doca_compress_task_decompress_lz4_block *raw_decompress_lz4_block_task,
        doca_data task_data,
        doca_data ctx_data);
    static void decompress_lz4_block_error_cb(
        doca_compress_task_decompress_lz4_block *raw_decompress_lz4_block_task,
        doca_data task_data,
        doca_data ctx_data);
    static void decompress_deflate_cb(
        doca_compress_task_decompress_deflate *raw_decompress_deflate_task,
        doca_data task_data,
        doca_data ctx_data);
    static void decompress_deflate_error_cb(
        doca_compress_task_decompress_deflate *raw_decompress_deflate_task,
        doca_data task_data,
        doca_data ctx_data);
    
    doca_compress *m_inner{};

    Lz4BlockDecompressTask::ContextCompleteCallback m_lz4_block_decompress_cb{
        [](std::unique_ptr<Lz4BlockDecompressTask>) {}};
    Lz4BlockDecompressTask::ContextCompleteCallback m_lz4_block_decompress_error_cb{
        [](std::unique_ptr<Lz4BlockDecompressTask>) {
        throw std::runtime_error("decompress error");
    }};

    DeflateDecompressTask::ContextCompleteCallback m_deflate_decompress_cb{
        [](std::unique_ptr<DeflateDecompressTask>) {}};
    DeflateDecompressTask::ContextCompleteCallback m_deflate_decompress_error_cb{
        [](std::unique_ptr<DeflateDecompressTask>) {
        throw std::runtime_error("decompress error");
    }};
};
}
