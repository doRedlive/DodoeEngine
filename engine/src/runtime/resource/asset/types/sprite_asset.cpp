// do@Redlive

#include "sprite_asset.h"

#include "runtime/resource/asset/importer/import_settings_io.h"

namespace dodoe {

    Bool SpriteAsset::loadFromSource(const String& absolute_source_path) {
        ImportSettings settings;
        if (!ImportSettingsIO::Load(FsPath(absolute_source_path.c_str()), settings)) {
            return false;
        }

        if (settings.settings.contains("pixelsPerUnit") && settings.settings["pixelsPerUnit"].is_number()) {
            m_pixels_per_unit = settings.settings["pixelsPerUnit"].get<Float>();
        }
        if (settings.settings.contains("pivot") && settings.settings["pivot"].is_array()
            && settings.settings["pivot"].size() >= 2) {
            m_pivot.x = settings.settings["pivot"][0].get<Float>();
            m_pivot.y = settings.settings["pivot"][1].get<Float>();
        }
        if (settings.settings.contains("slice") && settings.settings["slice"].is_array()
            && settings.settings["slice"].size() >= 4) {
            m_slice.left = settings.settings["slice"][0].get<Float>();
            m_slice.bottom = settings.settings["slice"][1].get<Float>();
            m_slice.right = settings.settings["slice"][2].get<Float>();
            m_slice.top = settings.settings["slice"][3].get<Float>();
        }

        m_meta.source_path = absolute_source_path;
        return true;
    }

    void SpriteAsset::unloadRuntime() {
    }

} // dodoe
