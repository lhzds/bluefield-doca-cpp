#include "docacxx/compress.h"
#include <doca_compress.h>
#include <doca_error.h>
#include <doca_pe.h>
#include <iostream>

namespace docacxx {
Compress::Compress(doca_pe *pe, doca_dev *dev, const Cfg &cfg) {
    check_derr(doca_compress_create(dev, &inner),
        "failed to create compress consumer");
    init_ctx(doca_compress_as_ctx(inner), pe);
}

void Compress::clean_impl() {
    auto result = doca_compress_destroy(inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy compress properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
}

Lz4Compress::Lz4Compress(doca_pe *pe, doca_dev *dev, const Cfg &cfg)
    :Compress(pe, dev, cfg) {
    check_derr(doca_compress_task_decompress_lz4_block_set_conf(inner,
        decompress_lz4_block_cb, decompress_lz4_block_error_cb,
        cfg.decompress_task_num), "failed to create compress");
}

void Lz4Compress::decompress_lz4_block_cb(
    doca_compress_task_decompress_lz4_block *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_compress_task_decompress_lz4_block_as_task(task));
    auto *compress = (Lz4Compress *)ctx_user_data.ptr;
    compress->custom_decompress_cb(task_user_data);
}

void Lz4Compress::decompress_lz4_block_error_cb(
    doca_compress_task_decompress_lz4_block *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_compress_task_decompress_lz4_block_as_task(task));
    auto *compress = (Lz4Compress *)ctx_user_data.ptr;
    compress->custom_decompress_error_cb(task_user_data);
}

doca_task *Lz4Compress::alloc_decompress_task(const doca_buf *src_buff,
    doca_buf *dst_buff, doca_data task_user_data) {
    doca_compress_task_decompress_lz4_block *compress_task{};
    auto result = doca_compress_task_decompress_lz4_block_alloc_init(inner, src_buff, dst_buff,
        task_user_data, &compress_task);
    if (result == DOCA_ERROR_NO_MEMORY) {
        return nullptr;
    }
    check_derr(result, "failed to alloc send_task");
    return doca_compress_task_decompress_lz4_block_as_task(compress_task);
}

DeflateCompress::DeflateCompress(doca_pe *pe, doca_dev *dev, const Cfg &cfg)
    :Compress(pe, dev, cfg) {
    check_derr(doca_compress_task_decompress_deflate_set_conf(inner,
        decompress_deflate_cb, decompress_deflate_error_cb,
        cfg.decompress_task_num), "failed to create compress");
}

void DeflateCompress::decompress_deflate_cb(
    doca_compress_task_decompress_deflate *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_compress_task_decompress_deflate_as_task(task));
    auto *compress = (DeflateCompress *)ctx_user_data.ptr;
    compress->custom_decompress_cb(task_user_data);
}

void DeflateCompress::decompress_deflate_error_cb(
    doca_compress_task_decompress_deflate *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    doca_task_free(doca_compress_task_decompress_deflate_as_task(task));
    auto *compress = (DeflateCompress *)ctx_user_data.ptr;
    compress->custom_decompress_cb(task_user_data);
}

doca_task *DeflateCompress::alloc_decompress_task(const doca_buf *src_buff,
    doca_buf *dst_buff, doca_data task_user_data) {
    doca_compress_task_decompress_deflate *compress_task{};
    auto result = doca_compress_task_decompress_deflate_alloc_init(inner, src_buff, dst_buff,
        task_user_data, &compress_task);
    if (result == DOCA_ERROR_NO_MEMORY) {
        return nullptr;
    }
    check_derr(result, "failed to alloc send_task");
    return doca_compress_task_decompress_deflate_as_task(compress_task);
}
}
