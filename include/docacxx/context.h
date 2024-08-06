#pragma once

#include <doca_ctx.h>
#include <doca_error.h>
#include <doca_pe.h>
#include <functional>
#include <iostream>

#include "doca_utils.h"

namespace docacxx {
class ContextBase {
public:
    using CustomStateChangedCb = std::function<void(doca_ctx_states prev_state,
						doca_ctx_states next_state)>;

    doca_ctx *ctx{};
    doca_pe *pe{};
    doca_ctx_states state{DOCA_CTX_STATE_IDLE};

    virtual ~ContextBase() = default;

    void init_ctx(doca_ctx *ctx_, doca_pe *pe_) {
        ctx = ctx_;
        doca_data data{};
        data.ptr = this;
        check_derr(doca_ctx_set_user_data(ctx, data), "failed to set user data of ctx");
        check_derr(doca_ctx_set_state_changed_cb(ctx, state_changed_cb),
            "failed to set state changed cb");
        
        pe = pe_;
        check_derr(doca_pe_connect_ctx(pe, ctx), "failed to connect pe with ctx");
    }

    void start() {
        check_derr(doca_ctx_start(ctx), "Failed to start ctx");
    }

    void stop() {
        auto result = doca_ctx_stop(ctx);
        if (result != DOCA_SUCCESS || result != DOCA_ERROR_IN_PROGRESS) {
            check_derr(result, "Unable to stop server ctx");
        }
    }

    bool submit(doca_task *task) {
        doca_error result{doca_task_submit(task)};
        if (result != DOCA_ERROR_AGAIN) {
            check_derr(result, "failed to summit send_task");
        }
        return result != DOCA_ERROR_AGAIN;
    }

    bool is_cleaned() { return is_cleaned_; }
    bool clean() {
        if (state != DOCA_CTX_STATE_IDLE) {
            return false;
        }
        clean_impl();
        return true;
    }

    CustomStateChangedCb custom_stated_change_cb{[](doca_ctx_states prev_state,
						doca_ctx_states next_state) {}};

protected:
    virtual void clean_impl() = 0;

private:
    bool is_cleaned_{};

    static void state_changed_cb(const doca_data user_data,
						doca_ctx *ctx,
						doca_ctx_states prev_state,
						doca_ctx_states next_state) {
        auto *context = (ContextBase *)user_data.ptr; // 子类指针转为父类
        context->state = next_state;
        context->custom_stated_change_cb(prev_state, next_state);
    };
};
}