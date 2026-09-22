// do@Redlive

#include "primitive_scene_info.h"

namespace dodoe {

    void PrimitiveSceneInfo::advanceMotionFrame() const {
        if (m_motion_frame_id == s_motion_frame_counter) {
            return;
        }
        m_motion_frame_id = s_motion_frame_counter;
        const Size_t prev_count = m_prev_frame_instance_data.size();
        for (Size_t i = 0; i < m_instance_scene_data.size(); ++i) {
            m_instance_scene_data[i].prev_model = (i < prev_count)
                ? m_prev_frame_instance_data[i].model
                : m_instance_scene_data[i].model;
        }
        m_prev_frame_instance_data = m_instance_scene_data;
    }

    Bool PrimitiveSceneInfo::hasRelevantBatch(const MeshPassType pass_type) const {
        for (const auto& batch : m_mesh_batches) {
            if (batch.isValid() && batch.isRelevant(pass_type)) {
                return true;
            }
        }
        return false;
    }

} // dodoe