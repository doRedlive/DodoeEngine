// do@Redlive

#include "texture_asset.h"

namespace dodoe {

    Bool TextureAsset::loadFromSource(const String& absolute_source_path) {
        m_blob.load(absolute_source_path, m_flip_vertical);
        if (!m_blob.isValid()) {
            return false;
        }
        m_meta.source_path = absolute_source_path;
        return true;
    }

    void TextureAsset::unloadRuntime() {
        m_blob.free();
        m_gpu_texture.reset();
    }

} // dodoe
