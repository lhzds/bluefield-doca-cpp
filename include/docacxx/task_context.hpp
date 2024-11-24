#pragma once

#include <functional>
#include <memory>
#include <queue>
#include <docacxx/task.hpp>

namespace docacxx {
template<typename Task>
class TaskContext {
public:
    explicit TaskContext(size_t task_slot_num): m_task_slot_num(task_slot_num) {}

    void reset() {
        m_complete_cnt = 0;
        m_processing_task_cnt = 0;
        if (not m_wait_queue.empty()) {
            m_wait_queue = std::queue<std::function<void()>>();
        }
    }

    size_t task_slot_num() const {
        return m_task_slot_num;
    }

    size_t complete_cnt() const {
        return m_complete_cnt;
    }

    size_t processing_task_cnt() const {
        return m_processing_task_cnt;
    }

    Task::ContextCompleteCallback make_pipeline_submitter() {
        return [&](std::unique_ptr<Task> completed_task) {
            completed_task.reset(); // 提前释放刚完成的 task

            if (not m_wait_queue.empty()) {
                auto &submit_func = m_wait_queue.front();
                submit_func();
                m_wait_queue.pop();
            } else {
                --m_processing_task_cnt;
            }
            ++m_complete_cnt;
        };
        
    };

    // 当需要传递 简单的 Lambda 表达式 或 函数指针 时，使用模板版本，避免额外的动态分配和类型擦除
    template <typename SubmitFunc>
    void submit(SubmitFunc&& submit_func) {
        if (m_processing_task_cnt >= m_task_slot_num) {
            m_wait_queue.emplace([=]() { submit_func(); });
            return;
        }

        if constexpr (std::is_invocable_v<SubmitFunc>) {
            submit_func();
        } else {
            static_assert(std::is_invocable_v<SubmitFunc>, "SubmitFunc must be invocable");
        }
        ++m_processing_task_cnt;
    }

    using SubmitFunc = std::function<void()>;

    void submit(SubmitFunc submit_func) {
        if (m_processing_task_cnt >= m_task_slot_num) {
            m_wait_queue.emplace(std::move(submit_func));
            return;
        }
        submit_func();
        ++m_processing_task_cnt;
    }

private:
    size_t m_complete_cnt{};
    size_t m_processing_task_cnt{};
    const size_t m_task_slot_num{};
    std::queue<std::function<void()>> m_wait_queue;
};
}
