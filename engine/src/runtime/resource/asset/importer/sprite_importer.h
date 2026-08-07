// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/importer/asset_importer.h"
#include "runtime/resource/asset/types/sprite_asset.h"

namespace dodoe {

    class SpriteImporter : public AssetImporter {
    public:
        [[nodiscard]] const char* getName() const override { return "SpriteImporter"; }
        [[nodiscard]] Json getDefaultSettings() const override {
            return Json{{"pixelsPerUnit", 100.0f},
                        {"pivot", Json::array({0.5f, 0.5f})},
                        {"slice", Json::array({0.0f, 0.0f, 0.0f, 0.0f})}};
        }
        Scope<Asset> import(const ImportContext& ctx) override;
    };

} // dodoe
