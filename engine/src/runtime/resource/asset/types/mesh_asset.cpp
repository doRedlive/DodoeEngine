// do@Redlive

#include "mesh_asset.h"

namespace dodoe {

    Bool MeshAsset::loadFromSource(const String& absolute_source_path) {
        m_blob.load(absolute_source_path);
        if (!m_blob.isValid()) {
            return false;
        }
        m_meta.source_path = absolute_source_path;
        return true;
    }

    void MeshAsset::unloadRuntime() {
        m_blob.free();
    }

} // dodoe
