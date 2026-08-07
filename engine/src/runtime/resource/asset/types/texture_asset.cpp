// do@Redlive

#include "texture_asset.h"

#include "runtime/resource/asset/importer/import_settings_io.h"

namespace dodoe {

    Bool TextureAsset::loadFromSource(const String& absolute_source_path) {
        ImportSettings settings;
        if (ImportSettingsIO::Load(FsPath(absolute_source_path.c_str()), settings)) {
            if (settings.settings.contains("flipVertical") && settings.settings["flipVertical"].is_boolean()) {
                m_flip_vertical = settings.settings["flipVertical"].get<Bool>();
            }
            if (settings.settings.contains("pixelsPerUnit") && settings.settings["pixelsPerUnit"].is_number()) {
                m_ppu = settings.settings["pixelsPerUnit"].get<Float>();
            }
        }

        m_blob.load(absolute_source_path, m_flip_vertical);
        if (!m_blob.isValid()) {
            return false;
        }
        m_meta.source_path = absolute_source_path;
        return true;
    }

    void TextureAsset::unloadRuntime() {
        m_blob.free();
        m_gpu_texture = {};
    }

} // dodoe
