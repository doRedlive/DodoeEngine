// do@Redlive

#include "mesh_draw_types.h"

namespace dodoe {

    void MeshPassRelevance::setRelevant(const MeshPassType pass_type, const Bool relevant) {
        pass_relevance[static_cast<Size_t>(pass_type)] = relevant;
    }

    Bool MeshPassRelevance::isRelevant(const MeshPassType pass_type) const {
        return pass_relevance[static_cast<Size_t>(pass_type)];
    }

} // dodoe
