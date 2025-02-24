#pragma once

#include "docacxx/mem.h"
#include "docacxx/noncopyable.h"
#include "doca_utils.h"
#include <doca_comch.h>
#include <doca_comch_consumer.h>
#include <doca_comch_producer.h>
#include <doca_compress.h>
#include <doca_dma.h>
#include <doca_erasure_coding.h>
#include <doca_error.h>
#include <doca_pe.h>
#include <doca_types.h>
#include <functional>
#include <memory>
#include <type_traits>

namespace docacxx {
template<typename RawDocaTask>
class Task: public Noncopyable {
public:
    using CompleteCallback = std::function<void(const Task &)>;
    // 使用 std::unique_ptr，离开作用域自动析构
    using ContextCompleteCallback = std::function<void(std::unique_ptr<Task>)>;

    Task(RawDocaTask *inner, CompleteCallback cb):
        m_inner(inner), m_cb(std::move(cb)) {
        doca_data task_data;
        task_data.ptr = this;
        doca_task_set_user_data(raw_base(), task_data);
    };
    ~Task() { 
        if (m_inner) {
            doca_task_free(raw_base());
        }
    }

    RawDocaTask *raw() const {
        return m_inner;
    }

    doca_task *raw_base() const {
        if constexpr (std::is_same_v<RawDocaTask, doca_comch_task_send>) {
            return doca_comch_task_send_as_task(m_inner);
        } else if constexpr (std::is_same_v<RawDocaTask, doca_compress_task_decompress_lz4_block>) {
            return doca_compress_task_decompress_lz4_block_as_task(m_inner);
        } else if constexpr (std::is_same_v<RawDocaTask, doca_compress_task_decompress_deflate>) {
            return doca_compress_task_decompress_deflate_as_task(m_inner);
        } else if constexpr (std::is_same_v<RawDocaTask, doca_dma_task_memcpy>) {
            return doca_dma_task_memcpy_as_task(m_inner);
        } else if constexpr (std::is_same_v<RawDocaTask, doca_ec_task_update>) {
            return doca_ec_task_update_as_task(m_inner);
        } else if constexpr (std::is_same_v<RawDocaTask, doca_ec_task_create>) {
            return doca_ec_task_create_as_task(m_inner);
        } else if constexpr (std::is_same_v<RawDocaTask, doca_ec_task_recover>) {
            return doca_ec_task_recover_as_task(m_inner);
        } else if constexpr (std::is_same_v<RawDocaTask, doca_comch_producer_task_send>) {
            return doca_comch_producer_task_send_as_task(m_inner);
        } else if constexpr (std::is_same_v<RawDocaTask, doca_comch_task_send>) {
            return doca_comch_task_send_as_task(m_inner);
        } else {
            throw_with("Unsupported doca_task type");
        }
    }

    doca_error_t status() const {
        return doca_task_get_status(raw_base());
    }

    void set_cb(CompleteCallback cb) {
        m_cb = std::move(cb);
    }

    void call_cb() {
        if (m_cb) {
            m_cb(*this);
        }
    }

private:
    RawDocaTask *m_inner{};
    CompleteCallback m_cb{}; 
};

template<>
class Task<doca_comch_consumer_task_post_recv>: public Noncopyable {
public:
    using CompleteCallback = std::function<void(const Task &)>;
    // 使用 std::unique_ptr，离开作用域自动析构
    using ContextCompleteCallback = std::function<void(std::unique_ptr<Task>)>;

    Task(doca_comch_consumer_task_post_recv *inner, Buffer *recv_buf, CompleteCallback cb):
        m_inner(inner), m_cb(std::move(cb)), m_recv_buf(recv_buf) {
        doca_data task_data;
        task_data.ptr = this;
        doca_task_set_user_data(raw_base(), task_data);
    };
    ~Task() { 
        if (m_inner) {
            doca_task_free(raw_base());
        }
    }

    doca_comch_consumer_task_post_recv *raw() const {
        return m_inner;
    }

    doca_task *raw_base() const {
        return doca_comch_consumer_task_post_recv_as_task(m_inner);
    }

    void set_cb(CompleteCallback cb) {
        m_cb = std::move(cb);
    }

    void call_cb() {
        if (m_cb) {
            m_cb(*this);
        }
    }

    char *imm_data() const {
        return (char *)doca_comch_consumer_task_post_recv_get_imm_data(raw());
    }

    size_t imm_data_size() const {
        return doca_comch_consumer_task_post_recv_get_imm_data_len(raw());
    }

    Buffer *buf() const {
        return m_recv_buf;
    }

private:
    doca_comch_consumer_task_post_recv *m_inner{};
    CompleteCallback m_cb{};
    Buffer *m_recv_buf{};
};
}
