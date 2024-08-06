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

    check_derr(doca_compress_task_decompress_lz4_block_set_conf(inner,
        decompress_lz4_block_cb, decompress_lz4_block_error_cb,
        cfg.decompress_lz4_block_task_num), "failed to create compress");
}

void Compress::clean_impl() {
    auto result = doca_compress_destroy(inner);
    if (result != DOCA_SUCCESS) {
        std::cerr << "Failed to destroy compress properly with error = "
                  << doca_error_get_name(result) << std::endl;
    }
}

void Compress::decompress_lz4_block_cb(
    doca_compress_task_decompress_lz4_block *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    auto *compress = (Compress *)ctx_user_data.ptr;
    compress->custom_decompress_lz4_block_cb(task_user_data);
    doca_task_free(doca_compress_task_decompress_lz4_block_as_task(task));
}

void Compress::decompress_lz4_block_error_cb(
    doca_compress_task_decompress_lz4_block *task,
    doca_data task_user_data,
    doca_data ctx_user_data) {
    auto *compress = (Compress *)ctx_user_data.ptr;
    compress->custom_decompress_lz4_block_error_cb(task_user_data);
    doca_task_free(doca_compress_task_decompress_lz4_block_as_task(task));
}

doca_task *Compress::alloc_decompress_lz4_block_task(const doca_buf *src_buff, doca_buf *dst_buff,
    doca_data task_user_data) {
    doca_compress_task_decompress_lz4_block *compress_task{};
    auto result = doca_compress_task_decompress_lz4_block_alloc_init(inner, src_buff, dst_buff,
        task_user_data, &compress_task);
    if (result == DOCA_ERROR_NO_MEMORY) {
        return nullptr;
    }
    check_derr(result, "failed to alloc send_task");
    return doca_compress_task_decompress_lz4_block_as_task(compress_task);
}
}
