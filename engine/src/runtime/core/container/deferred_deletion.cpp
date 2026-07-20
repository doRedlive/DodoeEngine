// do@Redlive

#include "deferred_deletion.h"

namespace dodoe {

    void DeferredDeletionQueue::processCompleted(UInt64 last_completed_frame) {
        Size_t keep_start = 0;
        Bool found_keep = false;
        for (Size_t i = 0; i < m_queue.size(); i++) {
            if (m_queue[i].frame_number <= last_completed_frame) {
                m_queue[i].deleter();
            } else {
                if (!found_keep) {
                    keep_start = i;
                    found_keep = true;
                }
            }
        }
        if (found_keep) {
            m_queue.erase(m_queue.begin(), m_queue.begin() + keep_start);
        } else {
            m_queue.clear();
        }
    }

    void DeferredDeletionQueue::clear() {
        for (auto& entry : m_queue) {
            entry.deleter();
        }
        m_queue.clear();
    }

} // namespace dodoe
