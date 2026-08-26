// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/importer/asset_importer.h"
#include "runtime/resource/asset/types/tiled_map_asset.h"

namespace dodoe {

    class TiledMapImporter : public AssetImporter {
    public:
        [[nodiscard]] const char* getName() const override { return "TiledMapImporter"; }
        [[nodiscard]] Json getDefaultSettings() const override { return Json::object(); }
        Scope<Asset> import(const ImportContext& ctx) override;
    };

} // dodoe
