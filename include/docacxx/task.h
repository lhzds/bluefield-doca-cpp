#pragma once

#include "noncopyable.h"
#include <doca_pe.h>
#include <doca_types.h>
#include <functional>
#include <memory>

namespace docacxx {
class Task: public Noncopyable {
public:
    using CompleteCallback = std::function<void(const Task &)>;

    Task(doca_task *inner, CompleteCallback cb): m_inner(inner), m_cb(std::move(cb)) {
        doca_data task_data;
        task_data.ptr = this;
        doca_task_set_user_data(m_inner, task_data);
    };
    ~Task() { 
        if (m_inner) {
            doca_task_free(m_inner);
        }
    }

    doca_task *raw() {
        return m_inner;
    }

    void call_cb() {
        m_cb(*this);
    }

private:
    doca_task *m_inner{};
    CompleteCallback m_cb{}; 
};

// 使用 std::unique_ptr，离开作用域自动析构
using ContextTaskCompleteCallback = std::function<void(std::unique_ptr<Task>)>;
}
