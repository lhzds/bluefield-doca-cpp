#include "docacxx/ec.h"
#include "docacxx/mem.h"
#include <map>
#include <memory>
#include <queue>

class ErasureCoder {
public:
    using Cfg = docacxx::Ec::Cfg;
    ErasureCoder(doca_dev *dev, doca_ec_matrix_type mat_type,
        size_t data_block_count, size_t rdnc_block_count, size_t doca_max_create_task_num)
        : doca_max_create_task_num_(doca_max_create_task_num) {
        docacxx::check_derr(doca_pe_create(&pe_), "failed to create pe");

        docacxx::Ec::Cfg cfg;
        cfg.create_task_num = doca_max_create_task_num;
        cfg.recover_task_num = 1;
        ec_ = std::make_unique<docacxx::Ec>(pe_, dev, cfg);

        ec_->custom_create_cb = [&](doca_data task_user_data) {
            if (not wait_queue_.empty()) {
                auto [src_buf, dst_buf] = wait_queue_.front();
                wait_queue_.pop();
                auto *task = ec_->alloc_create_task(matrix_, src_buf, dst_buf);
                ec_->submit(task);
            }

            ++finished_;
        };

        ec_->custom_recover_cb = [&](doca_data task_user_data) {
            recover_done_ = true;
        };

        ec_->start();
        while (ec_->state != DOCA_CTX_STATE_RUNNING) {
            doca_pe_progress(pe_);
        }

        matrix_ = ec_->create_matrix(mat_type, data_block_count,
            rdnc_block_count);
        assert(matrix_);
    }

    ~ErasureCoder() {
        ec_->destroy_matrix(matrix_);

        ec_->stop();
        while (ec_->state != DOCA_CTX_STATE_IDLE) {
            doca_pe_progress(pe_);
        }
        ec_->clean();

        auto result = doca_pe_destroy(pe_);
        if (result != DOCA_SUCCESS) {
            std::cerr << "failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    void create(const std::vector<std::unique_ptr<docacxx::Buffer>> &src,
        std::vector<std::unique_ptr<docacxx::Buffer>> &dst) {
        finished_ = 0;
        assert(src.size() == dst.size());

        for (size_t i{}; i < std::min(src.size(), doca_max_create_task_num_); ++i) {
            auto *task = ec_->alloc_create_task(matrix_, src[i]->buf, dst[i]->buf);
            assert(task);
            ec_->submit(task);
        }

        if (src.size() > doca_max_create_task_num_) {
            for (size_t i = doca_max_create_task_num_; i < src.size(); ++i) {
                wait_queue_.emplace(src[i]->buf, dst[i]->buf);
            }
        }
        while (finished_ != src.size()) {
            doca_pe_progress(pe_);
        }
    }

    void recover(const std::unique_ptr<docacxx::Buffer> &src,
        std::unique_ptr<docacxx::Buffer> &dst,
        std::vector<uint32_t> &missing_indices) {
        recover_done_ = false;
        doca_ec_matrix *recover_matrix = ec_->create_recover_matrix(matrix_, missing_indices);
        assert(recover_matrix);
        auto *task = ec_->alloc_recover_task(recover_matrix, src->buf, dst->buf);
        assert(task);      
        ec_->submit(task);

        while (not recover_done_) {
            doca_pe_progress(pe_);
        }
        ec_->destroy_matrix(recover_matrix);
    }

private:
    doca_pe *pe_{};
    std::unique_ptr<docacxx::Ec> ec_;
    size_t finished_{};
    size_t doca_max_create_task_num_{};
    doca_ec_matrix *matrix_{};
    std::queue<std::pair<const doca_buf *, doca_buf *>> wait_queue_;

    bool recover_done_{};
};
