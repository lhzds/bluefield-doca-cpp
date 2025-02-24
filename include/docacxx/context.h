#pragma once

#include <doca_ctx.h>
#include <doca_error.h>
#include <doca_pe.h>
#include <functional>

#include "docacxx/doca_utils.h"
#include "docacxx/noncopyable.h"

namespace docacxx {
class Context: public Noncopyable {
public:
    using State = doca_ctx_states;
    using StateChangedCallback = std::function<void(State prev_state, State next_state)>;

    Context() = default;         
    virtual ~Context() = default;

    State state() const {
        return m_state;
    }

    doca_pe *pe() const {
        return m_pe;
    }

    void start() {
        check_derr(doca_ctx_start(m_inner), "Failed to start ctx");
    }

    void start_sync() {
        start();
        while (state() != DOCA_CTX_STATE_RUNNING) {
            doca_pe_progress(m_pe);
            if (m_other_pe && m_other_pe != m_pe) {
                doca_pe_progress(m_other_pe);
            }
        }
    }

    virtual void stop() {
        auto result = doca_ctx_stop(m_inner);
        if (result != DOCA_SUCCESS || result != DOCA_ERROR_IN_PROGRESS) {
            check_derr(result, "Unable to stop server ctx");
        }
    }

    void stop_sync() {
        stop();
        while (state() != DOCA_CTX_STATE_IDLE) {
            doca_pe_progress(m_pe);
        }
    }

    bool is_cleaned() { return m_is_cleaned; }
    
    bool clean() {
        if (m_state != DOCA_CTX_STATE_IDLE) {
            return false;
        }
        clean_impl();
        m_is_cleaned = true;
        return true;
    }

    void clean_sync() {
        stop_sync();
        clean();
    }

protected:
    void init_ctx(doca_ctx *inner, doca_pe *pe, doca_pe *other_pe = nullptr) {
        m_inner = inner;
        doca_data ctx_data{};
        ctx_data.ptr = this;
        check_derr(doca_ctx_set_user_data(m_inner, ctx_data), "Failed to set user data for ctx");
        check_derr(doca_ctx_set_state_changed_cb(m_inner, state_changed_cb),
            "Failed to set state changed cb for ctx");
        
        m_pe = pe;
        m_other_pe = other_pe;
        check_derr(doca_pe_connect_ctx(m_pe, m_inner), "Failed to connect pe with ctx");
    }

    virtual void clean_impl() = 0;

    // false 非空指针表示 retry
    bool submit_impl(doca_task *raw_task) {
        doca_error result{doca_task_submit(raw_task)};
        if (result == DOCA_ERROR_AGAIN) {
            return false;
        }

        check_derr(result, "Failed to summit task");
        return true;
    }

private:
    State m_state{DOCA_CTX_STATE_IDLE};
    doca_pe *m_pe{};
    doca_pe *m_other_pe{};
    doca_ctx *m_inner{};
    bool m_is_cleaned{};

    StateChangedCallback m_stated_change_cb{[](State, State) {}};

    static void state_changed_cb(const doca_data user_data,
						doca_ctx *ctx,
						State prev_state,
						State next_state) {
        auto *context = (Context *)user_data.ptr; // 子类指针转为父类
        context->m_state = next_state;
        context->m_stated_change_cb(prev_state, next_state);
    };
};
}
