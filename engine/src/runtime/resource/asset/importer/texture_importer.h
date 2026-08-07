// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/importer/asset_importer.h"
#include "runtime/resource/asset/types/texture_asset.h"

namespace dodoe {

    class TextureImporter : public AssetImporter {
    public:
        [[nodiscard]] const char* getName() const override { return "TextureImporter"; }
        [[nodiscard]] Json getDefaultSettings() const override {
            return Json{{"flipVertical", true}, {"pixelsPerUnit", 100.0f}};
        }
        Scope<Asset> import(const ImportContext& ctx) override;
    };

} // dodoe
