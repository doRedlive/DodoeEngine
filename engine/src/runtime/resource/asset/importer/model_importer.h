// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/importer/asset_importer.h"
#include "runtime/resource/asset/types/mesh_asset.h"

namespace dodoe {

    class ModelImporter : public AssetImporter {
    public:
        [[nodiscard]] const char* getName() const override { return "ModelImporter"; }
        Scope<Asset> import(const ImportContext& ctx) override;
    };

} // dodoe
