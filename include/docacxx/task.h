#pragma once

#include "noncopyable.h"
#include <doca_pe.h>
#include <doca_types.h>
#include <functional>

namespace docacxx {
class Task: public Noncopyable {
public:
    using CompleteCallback = std::function<void()>;

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

    const CompleteCallback &callback() {
        return m_cb;
    }

private:
    doca_task *m_inner{};
    CompleteCallback m_cb{}; 
};
}
