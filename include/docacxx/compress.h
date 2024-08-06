#pragma once

#include "docacxx/context.h"
#include <doca_compress.h>
#include <doca_pe.h>
#include <functional>

namespace docacxx {
class Compress: public ContextBase {
public:
    using CustomCompressCb = std::function<void(doca_data task_user_data)>;

    struct Cfg {
        size_t decompress_lz4_block_task_num{};  
    };

    Compress(doca_pe *pe, doca_dev *dev, const Cfg &cfg);
    ~Compress() = default;

    doca_compress *inner{};

    CustomCompressCb custom_decompress_lz4_block_cb{[](doca_data task_user_data) {}};
    CustomCompressCb custom_decompress_lz4_block_error_cb{[](doca_data task_user_data) {}};

    doca_task *alloc_decompress_lz4_block_task(const doca_buf *src_buff, doca_buf *dst_buff,
        doca_data user_data = doca_data{});

protected:
    void clean_impl() override;

private:
    static void decompress_lz4_block_cb(
        doca_compress_task_decompress_lz4_block *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
    static void decompress_lz4_block_error_cb(
        doca_compress_task_decompress_lz4_block *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
};
}
