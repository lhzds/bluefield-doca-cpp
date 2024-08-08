#include "docacxx/ec.h"
#include "docacxx/mem.h"
#include <queue>

class ErasureCoder {
public:
    using Cfg = docacxx::Ec::Cfg;
    ErasureCoder(doca_dev *dev, doca_ec_matrix_type mat_type,
        size_t data_block_count, size_t rdnc_block_count, const Cfg &cfg)
        :cfg_{cfg} {
        docacxx::check_derr(doca_pe_create(&pe_), "failed to create pe");

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

        ec_->start();
        while (ec_->state != DOCA_CTX_STATE_RUNNING) {
            doca_pe_progress(pe_);
        }

        matrix_ = ec_->create_matrix(mat_type, data_block_count,
            rdnc_block_count);
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

        for (size_t i{}; i < std::min(src.size(), cfg_.create_task_num); ++i) {
            auto *task = ec_->alloc_create_task(matrix_, src[i]->buf, dst[i]->buf);
            ec_->submit(task);
        }

        if (src.size() > cfg_.create_task_num) {
            for (size_t i = cfg_.create_task_num; i < src.size(); ++i) {
                wait_queue_.emplace(src[i]->buf, dst[i]->buf);
            }
        }
        while (finished_ != src.size()) {
            doca_pe_progress(pe_);
        }
    }

private:
    doca_pe *pe_{};
    Cfg cfg_{};
    std::unique_ptr<docacxx::Ec> ec_;
    size_t finished_{};
    doca_ec_matrix *matrix_;
    std::queue<std::pair<const doca_buf *, doca_buf *>> wait_queue_;
};
