#pragma once

#include <queue>
#include <functional>

class TaskContext {
public:
    explicit TaskContext(size_t task_slot_num): m_task_slot_num(task_slot_num) {}

    void reset() {
        m_complete_cnt = 0;
        if (not m_wait_queue.empty()) {
            m_wait_queue = SubmitEventQueue();
        }
    }

    size_t task_slot_num() const {
        return m_task_slot_num;
    }

    size_t complete_cnt() const {
        return m_complete_cnt;
    }

    std::function<void()> make_pipeline_submitter() {
        return [&] {
            if (not m_wait_queue.empty()) {
                auto &handler = m_wait_queue.front();
                handler();
                m_wait_queue.pop();
            }
            ++m_complete_cnt;
        };
    };

    void add_submit_event(std::function<void()> event) {
        m_wait_queue.emplace(std::move(event));
    }

private:
    using SubmitEventQueue = std::queue<std::function<void()>>;

    size_t m_complete_cnt{};
    const size_t m_task_slot_num{};
    SubmitEventQueue m_wait_queue;
};
