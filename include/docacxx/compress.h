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
        size_t decompress_task_num{};
    };

    Compress(doca_pe *pe, doca_dev *dev, const Cfg &cfg);
    virtual ~Compress() = default;

    doca_compress *inner{};

    CustomCompressCb custom_decompress_cb{[](doca_data task_user_data) {}};
    CustomCompressCb custom_decompress_error_cb{[](doca_data task_user_data) {}};

    virtual doca_task *alloc_decompress_task(const doca_buf *src_buff, doca_buf *dst_buff,
        doca_data user_data = doca_data{}) = 0;

protected:
    void clean_impl() override;
};

class Lz4Compress: public Compress {
public:
    Lz4Compress(doca_pe *pe, doca_dev *dev, const Cfg &cfg);
    ~Lz4Compress() = default;

    doca_task *alloc_decompress_task(const doca_buf *src_buff, doca_buf *dst_buff,
        doca_data user_data = doca_data{}) override;

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

class DeflateCompress: public Compress {
public:
    DeflateCompress(doca_pe *pe, doca_dev *dev, const Cfg &cfg);
    ~DeflateCompress() = default;

    doca_task *alloc_decompress_task(const doca_buf *src_buff, doca_buf *dst_buff,
        doca_data user_data = doca_data{}) override;

private:
    static void decompress_deflate_cb(
        doca_compress_task_decompress_deflate *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
    static void decompress_deflate_error_cb(
        doca_compress_task_decompress_deflate *task,
        doca_data task_user_data,
        doca_data ctx_user_data);
};
}
