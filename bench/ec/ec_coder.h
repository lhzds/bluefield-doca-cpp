#include <memory>

#include "docacxx/task_context.h"
#include "docacxx/ctx/ec.h"
#include "docacxx/mem.h"

class EcCoder {
public:
    using Cfg = docacxx::Ec::Cfg;
    EcCoder(doca_dev *dev, doca_ec_matrix_type mat_type,
        size_t data_block_count, size_t rdnc_block_count,
        size_t create_task_slot_num, size_t recover_task_slot_num)
        : m_create_ctx(create_task_slot_num), m_recover_ctx(recover_task_slot_num) {
        docacxx::check_derr(doca_pe_create(&m_pe), "failed to create pe");
        docacxx::Ec::Cfg cfg;
        cfg.m_create_task_slot_num = create_task_slot_num;
        cfg.m_recover_task_slot_num = recover_task_slot_num;
        m_inner = std::make_unique<docacxx::Ec>(m_pe, dev, cfg);
        m_inner->set_create_cb(m_create_ctx.make_pipeline_submitter());
        m_inner->set_recover_cb(m_recover_ctx.make_pipeline_submitter());
        m_inner->start_sync();

        m_create_matrix = m_inner->create_matrix(mat_type, data_block_count,
            rdnc_block_count);
    }

    ~EcCoder() {
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
        m_create_ctx.reset();

        for (size_t i{}; i < src.size(); ++i) {
            m_create_ctx.submit([&, i] {
                auto task = m_inner->alloc_create_task(m_create_matrix, *src[i], *dst[i], nullptr);
                m_inner->submit(std::move(task));
            });
        }
        while (m_create_ctx.complete_cnt() < src.size()) {
            doca_pe_progress(m_pe);
        }
    }

    void recover(const std::vector<std::unique_ptr<docacxx::Buffer>> &src,
        std::vector<std::unique_ptr<docacxx::Buffer>> &dst,
        std::vector<uint32_t> &missing_indices) {
        assert(src.size() == dst.size());
        m_recover_ctx.reset();
        doca_ec_matrix *recover_matrix = m_inner->create_recover_matrix(m_create_matrix, missing_indices);
        assert(recover_matrix);

        for (size_t i{}; i < src.size(); ++i) {
            m_recover_ctx.submit([&, i] {
                auto task = m_inner->alloc_recover_task(recover_matrix, *src[i], *dst[i], nullptr);
                m_inner->submit(std::move(task));
            });
        }
        while (m_recover_ctx.complete_cnt() != src.size()) {
            doca_pe_progress(m_pe);
        }
        m_inner->destroy_matrix(recover_matrix);
    }

private:
    doca_pe *m_pe{};
    std::unique_ptr<docacxx::Ec> m_inner;

    doca_ec_matrix *m_create_matrix{};
    TaskContext m_create_ctx;
    TaskContext m_recover_ctx;
};
