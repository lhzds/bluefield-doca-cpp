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
}

void Compress::clean_impl() {
    auto result = doca_compress_destroy(m_inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy compress properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
}

Lz4Compress::Lz4Compress(doca_pe *pe, doca_dev *dev, const Cfg &cfg)
    :Compress(pe, dev, cfg) {
    check_derr(doca_compress_task_decompress_lz4_block_set_conf(m_inner,
        decompress_lz4_block_cb, decompress_lz4_block_error_cb,
        cfg.m_decompress_task_slot_num), "failed to set lz4 block decompress task conf");
}

std::unique_ptr<Task> Lz4Compress::alloc_decompress_task(const Buffer &src, Buffer &dst,
Task::CompleteCallback cb) {
    doca_compress_task_decompress_lz4_block *task{};
    check_derr(doca_compress_task_decompress_lz4_block_alloc_init(m_inner, src.raw(),
        dst.raw(), doca_data{}, &task), "failed to alloc lz4 block decompress task");
    return std::make_unique<Task>(doca_compress_task_decompress_lz4_block_as_task(task), std::move(cb));
}

void Lz4Compress::decompress_lz4_block_cb(
    doca_compress_task_decompress_lz4_block *raw_lz4_block_decompress_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto *lz4_compress = (Lz4Compress *)ctx_data.ptr;
    std::unique_ptr<Task> task;
    task.reset((Task *)task_data.ptr);
    task->callback(); // 任务成功就会调用回调
    task.reset();

    lz4_compress->m_decompress_cb();
}

void Lz4Compress::decompress_lz4_block_error_cb(
    doca_compress_task_decompress_lz4_block *raw_lz4_block_decompress_task,
    doca_data task_data,
    doca_data ctx_data) {
    auto *lz4_compress = (Lz4Compress *)ctx_data.ptr;
    std::unique_ptr<Task> task;
    task.reset((Task *)task_data.ptr);
    task.reset();

    lz4_compress->m_decompress_error_cb();
}

DeflateCompress::DeflateCompress(doca_pe *pe, doca_dev *dev, const Cfg &cfg)
    :Compress(pe, dev, cfg) {
    check_derr(doca_compress_task_decompress_deflate_set_conf(m_inner,
        decompress_deflate_cb, decompress_deflate_error_cb,
        cfg.m_decompress_task_slot_num), "failed to set deflate decompression task conf");
}

std::unique_ptr<Task> DeflateCompress::alloc_decompress_task(const Buffer &src, Buffer &dst,
Task::CompleteCallback cb) {
    doca_compress_task_decompress_deflate *task{};
    check_derr(doca_compress_task_decompress_deflate_alloc_init(m_inner, src.raw(),
        dst.raw(), doca_data{}, &task), "failed to alloc deflate decompress task");
    return std::make_unique<Task>(doca_compress_task_decompress_deflate_as_task(task), std::move(cb));
}

void DeflateCompress::decompress_deflate_cb(
    doca_compress_task_decompress_deflate *raw_deflate_decompress_task,
    doca_data task_data,
    doca_data ctx_data) {
    std::unique_ptr<Task> task;
    task.reset((Task *)task_data.ptr);
    task->callback(); // 任务成功就会调用回调
    task.reset();

    auto *deflate_compress = (DeflateCompress *)ctx_data.ptr;
    deflate_compress->m_decompress_cb();
}

void DeflateCompress::decompress_deflate_error_cb(
    doca_compress_task_decompress_deflate *raw_deflate_decompress_task,
    doca_data task_data,
    doca_data ctx_data) {
    std::unique_ptr<Task> task;
    task.reset((Task *)task_data.ptr);
    task.reset();

    auto *deflate_compress = (DeflateCompress *)ctx_data.ptr;
    deflate_compress->m_decompress_error_cb();
}
}
