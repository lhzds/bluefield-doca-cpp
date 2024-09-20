#include "docacxx/ec.h"
#include "docacxx/mem.h"
#include <doca_error.h>
#include <memory>
#include <queue>

class ErasureCoder {
public:
    using Cfg = docacxx::Ec::Cfg;
    ErasureCoder(doca_dev *dev, doca_ec_matrix_type mat_type,
        size_t data_block_count, size_t rdnc_block_count,
        size_t doca_max_create_task_num, size_t doca_max_recover_task_num)
        : doca_max_create_task_num_(doca_max_create_task_num), doca_max_recover_task_num_(doca_max_recover_task_num) {
        docacxx::check_derr(doca_pe_create(&pe_), "failed to create pe");

        docacxx::Ec::Cfg cfg;
        cfg.create_task_num = doca_max_create_task_num;
        cfg.recover_task_num = doca_max_recover_task_num;
        ec_ = std::make_unique<docacxx::Ec>(pe_, dev, cfg);

        ec_->custom_create_cb = [&](doca_data task_user_data) {
            if (not create_wait_queue_.empty()) {
                auto [src_buf, dst_buf] = create_wait_queue_.front();
                create_wait_queue_.pop();
                auto *task = ec_->alloc_create_task(create_matrix_, src_buf, dst_buf);
                ec_->submit(task);
            }

            ++create_finished_count_;
        };
        ec_->custom_recover_cb = [&](doca_data task_user_data) {
            if (not recover_wait_queue_.empty()) {
                auto [recover_matrix, src_buf, dst_buf] = recover_wait_queue_.front();
                recover_wait_queue_.pop();
                auto *task = ec_->alloc_recover_task(recover_matrix, src_buf, dst_buf);
                ec_->submit(task);
            }

            ++recover_finished_count_;
        };

        ec_->start();
        while (ec_->state != DOCA_CTX_STATE_RUNNING) {
            doca_pe_progress(pe_);
        }

        create_matrix_ = ec_->create_matrix(mat_type, data_block_count,
            rdnc_block_count);
        assert(matrix_);
    }

    ~ErasureCoder() {
        ec_->destroy_matrix(create_matrix_);

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
        assert(src.size() == dst.size());

        create_finished_count_ = 0;

        for (size_t i{}; i < std::min(src.size(), doca_max_create_task_num_); ++i) {
            auto *task = ec_->alloc_create_task(create_matrix_, src[i]->buf, dst[i]->buf);
            assert(task);
            ec_->submit(task);
        }

        if (src.size() > doca_max_create_task_num_) {
            for (size_t i = doca_max_create_task_num_; i < src.size(); ++i) {
                create_wait_queue_.emplace(src[i]->buf, dst[i]->buf);
            }
        }
        while (create_finished_count_ < src.size()) {
            doca_pe_progress(pe_);
        }
    }

    void recover(const std::vector<std::unique_ptr<docacxx::Buffer>> &src,
        std::vector<std::unique_ptr<docacxx::Buffer>> &dst,
        std::vector<uint32_t> &missing_indices) {
        assert(src.size() == dst.size());

        recover_finished_count_ = 0;
        doca_ec_matrix *recover_matrix = ec_->create_recover_matrix(create_matrix_, missing_indices);
        assert(recover_matrix);

        for (size_t i{}; i < std::min(src.size(), doca_max_recover_task_num_); ++i) {
            auto *task = ec_->alloc_recover_task(recover_matrix, src[i]->buf, dst[i]->buf);
            assert(task);
            ec_->submit(task);
        }

        if (src.size() > doca_max_recover_task_num_) {
            for (size_t i = doca_max_recover_task_num_; i < src.size(); ++i) {
                recover_wait_queue_.emplace(recover_matrix, src[i]->buf, dst[i]->buf);
            }
        }

        while (recover_finished_count_ != src.size()) {
            doca_pe_progress(pe_);
        }
        ec_->destroy_matrix(recover_matrix);
    }

private:
    doca_pe *pe_{};
    std::unique_ptr<docacxx::Ec> ec_;

    doca_ec_matrix *create_matrix_{};
    size_t create_finished_count_{};
    size_t doca_max_create_task_num_{};
    std::queue<std::pair<const doca_buf *, doca_buf *>> create_wait_queue_;

    size_t recover_finished_count_{};
    size_t doca_max_recover_task_num_{};
    std::queue<std::tuple<doca_ec_matrix *, const doca_buf *, doca_buf *>> recover_wait_queue_;
};
