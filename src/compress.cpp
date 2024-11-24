#include "docacxx/ctx/compress.h"
#include <doca_compress.h>
#include <doca_error.h>
#include <doca_pe.h>
#include <iostream>

namespace docacxx {
Compress::Compress(doca_pe *pe, doca_dev *dev, const Cfg &cfg) {
    check_derr(doca_compress_create(dev, &m_inner),
        "failed to create compress context");
    init_ctx(doca_compress_as_ctx(m_inner), pe);

    check_derr(doca_compress_task_decompress_lz4_block_set_conf(m_inner,
        decompress_lz4_block_cb, decompress_lz4_block_error_cb,
        cfg.m_lz4_block_decompress_task_slot_num), "failed to set lz4 block decompress task conf");
    check_derr(doca_compress_task_decompress_deflate_set_conf(m_inner,
        decompress_deflate_cb, decompress_deflate_error_cb,
        cfg.m_deflate_decompress_task_slot_num), "failed to set deflate decompression task conf");
}

void Compress::clean_impl() {
    auto result = doca_compress_destroy(m_inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy compress properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
}

std::unique_ptr<Compress::Lz4BlockDecompressTask>
Compress::alloc_lz4_block_decompress_task(const Buffer &src, Buffer &dst,
    Lz4BlockDecompressTask::CompleteCallback cb) {
    doca_compress_task_decompress_lz4_block *task{};
    check_derr(doca_compress_task_decompress_lz4_block_alloc_init(m_inner, src.raw(),
        dst.raw(), doca_data{}, &task), "failed to alloc lz4 block decompress task");
    return std::make_unique<Compress::Lz4BlockDecompressTask>(
        doca_compress_task_decompress_lz4_block_as_task(task), task, std::move(cb));
}

std::unique_ptr<Compress::Lz4BlockDecompressTask>
Compress::submit(std::unique_ptr<Compress::Lz4BlockDecompressTask> task) {
    auto *released = task.release();
    if (not submit_impl(released->base_task())) {
        return std::unique_ptr<Compress::Lz4BlockDecompressTask>(released);
    }
    return {};
}

void Compress::decompress_lz4_block_cb(
    doca_compress_task_decompress_lz4_block *raw_lz4_block_decompress_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto task = std::unique_ptr<Compress::Lz4BlockDecompressTask>
        ((Compress::Lz4BlockDecompressTask *)task_data.ptr);
    task->call_cb(); // 任务成功就会调用回调

    auto *compress = (Compress *)ctx_data.ptr;
    compress->m_lz4_block_decompress_cb(std::move(task));
}

void Compress::decompress_lz4_block_error_cb(
    doca_compress_task_decompress_lz4_block *raw_lz4_block_decompress_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto task = std::unique_ptr<Compress::Lz4BlockDecompressTask>
        ((Compress::Lz4BlockDecompressTask *)task_data.ptr);

    auto *compress = (Compress *)ctx_data.ptr;
    compress->m_lz4_block_decompress_error_cb(std::move(task));
}

std::unique_ptr<Compress::DeflateDecompressTask>
Compress::alloc_deflate_decompress_task(const Buffer &src, Buffer &dst, DeflateDecompressTask::CompleteCallback cb) {
    doca_compress_task_decompress_deflate *task{};
    check_derr(doca_compress_task_decompress_deflate_alloc_init(m_inner, src.raw(),
        dst.raw(), doca_data{}, &task), "failed to alloc deflate decompress task");
    return std::make_unique<DeflateDecompressTask>(
        doca_compress_task_decompress_deflate_as_task(task), task, std::move(cb));
}

std::unique_ptr<Compress::DeflateDecompressTask>
Compress::submit(std::unique_ptr<Compress::DeflateDecompressTask> task) {
    auto *released = task.release();
    if (not submit_impl(released->base_task())) {
        return std::unique_ptr<Compress::DeflateDecompressTask>(released);
    }
    return {};
}

void Compress::decompress_deflate_cb(
    doca_compress_task_decompress_deflate *raw_deflate_decompress_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto task = std::unique_ptr<DeflateDecompressTask>((DeflateDecompressTask *)task_data.ptr);
    task->call_cb(); // 任务成功就会调用回调

    auto *compress = (Compress *)ctx_data.ptr;
    compress->m_deflate_decompress_cb(std::move(task));
}

void Compress::decompress_deflate_error_cb(
    doca_compress_task_decompress_deflate *raw_deflate_decompress_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto task = std::unique_ptr<DeflateDecompressTask>((DeflateDecompressTask *)task_data.ptr);

    auto *compress = (Compress *)ctx_data.ptr;
    compress->m_deflate_decompress_error_cb(std::move(task));
}
}
