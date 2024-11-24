#pragma once

#include "noncopyable.h"
#include <doca_pe.h>
#include <doca_types.h>
#include <functional>
#include <memory>

namespace docacxx {

template<typename RawDocaTask>
class Task: public Noncopyable {
public:
    using CompleteCallback = std::function<void(const Task &)>;
    // 使用 std::unique_ptr，离开作用域自动析构
    using ContextCompleteCallback = std::function<void(std::unique_ptr<Task>)>;

    Task(doca_task *base_task, RawDocaTask *inner, CompleteCallback cb):
        m_base_task(base_task), m_inner(inner), m_cb(std::move(cb)) {
        doca_data task_data;
        task_data.ptr = this;
        doca_task_set_user_data(m_base_task, task_data);
    };
    ~Task() { 
        if (m_inner) {
            doca_task_free(m_base_task);
        }
    }

    RawDocaTask *raw() {
        return m_inner;
    }

    doca_task *base_task() {
        return m_base_task;
    }

    void call_cb() {
        m_cb(*this);
    }

private:
    RawDocaTask *m_inner{};
    doca_task *m_base_task{};
    CompleteCallback m_cb{}; 
};
}
