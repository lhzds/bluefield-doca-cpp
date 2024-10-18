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
        : m_doca_max_create_task_num(doca_max_create_task_num), m_doca_max_recover_task_num(doca_max_recover_task_num) {
        docacxx::check_derr(doca_pe_create(&m_pe), "failed to create pe");

        docacxx::Ec::Cfg cfg;
        cfg.m_create_task_num = doca_max_create_task_num;
        cfg.m_recover_task_num = doca_max_recover_task_num;
        m_inner = std::make_unique<docacxx::Ec>(m_pe, dev, cfg);

        m_inner->m_custom_create_cb = [&](doca_data task_user_data) {
            if (not m_create_wait_queue.empty()) {
                auto [src_buf, dst_buf] = m_create_wait_queue.front();
                m_create_wait_queue.pop();
                auto *task = m_inner->alloc_create_task(m_create_matrix, src_buf, dst_buf);
                m_inner->submit(task);
            }

            ++m_create_finished_count;
        };
        m_inner->m_custom_recover_cb = [&](doca_data task_user_data) {
            if (not m_recover_wait_queue.empty()) {
                auto [recover_matrix, src_buf, dst_buf] = m_recover_wait_queue.front();
                m_recover_wait_queue.pop();
                auto *task = m_inner->alloc_recover_task(recover_matrix, src_buf, dst_buf);
                m_inner->submit(task);
            }

            ++m_recover_finished_count;
        };
        m_inner->start_sync();

        m_create_matrix = m_inner->create_matrix(mat_type, data_block_count,
            rdnc_block_count);
    }

    ~ErasureCoder() {
        m_inner->destroy_matrix(m_create_matrix);
        m_inner->clean_sync();

        auto result = doca_pe_destroy(m_pe);
        if (result != DOCA_SUCCESS) {
            std::cerr << "failed to destroy pe: " << doca_error_get_descr(result) << std::endl;
        }
    }

    void create(const std::vector<std::unique_ptr<docacxx::Buffer>> &src,
        std::vector<std::unique_ptr<docacxx::Buffer>> &dst) {
        assert(src.size() == dst.size());

        m_create_finished_count = 0;

        for (size_t i{}; i < std::min(src.size(), m_doca_max_create_task_num); ++i) {
            auto *task = m_inner->alloc_create_task(m_create_matrix, src[i]->m_inner, dst[i]->m_inner);
            m_inner->submit(task);
        }

        if (src.size() > m_doca_max_create_task_num) {
            for (size_t i = m_doca_max_create_task_num; i < src.size(); ++i) {
                m_create_wait_queue.emplace(src[i]->m_inner, dst[i]->m_inner);
            }
        }
        while (m_create_finished_count < src.size()) {
            doca_pe_progress(m_pe);
        }
    }

    void recover(const std::vector<std::unique_ptr<docacxx::Buffer>> &src,
        std::vector<std::unique_ptr<docacxx::Buffer>> &dst,
        std::vector<uint32_t> &missing_indices) {
        assert(src.size() == dst.size());

        m_recover_finished_count = 0;
        doca_ec_matrix *recover_matrix = m_inner->create_recover_matrix(m_create_matrix, missing_indices);
        assert(recover_matrix);

        for (size_t i{}; i < std::min(src.size(), m_doca_max_recover_task_num); ++i) {
            auto *task = m_inner->alloc_recover_task(recover_matrix, src[i]->m_inner, dst[i]->m_inner);
            m_inner->submit(task);
        }

        if (src.size() > m_doca_max_recover_task_num) {
            for (size_t i = m_doca_max_recover_task_num; i < src.size(); ++i) {
                m_recover_wait_queue.emplace(recover_matrix, src[i]->m_inner, dst[i]->m_inner);
            }
        }

        while (m_recover_finished_count != src.size()) {
            doca_pe_progress(m_pe);
        }
        m_inner->destroy_matrix(recover_matrix);
    }

private:
    doca_pe *m_pe{};
    std::unique_ptr<docacxx::Ec> m_inner;

    doca_ec_matrix *m_create_matrix{};
    size_t m_create_finished_count{};
    size_t m_doca_max_create_task_num{};
    std::queue<std::pair<const doca_buf *, doca_buf *>> m_create_wait_queue;

    size_t m_recover_finished_count{};
    size_t m_doca_max_recover_task_num{};
    std::queue<std::tuple<doca_ec_matrix *, const doca_buf *, doca_buf *>> m_recover_wait_queue;
};
