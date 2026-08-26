// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/importer/asset_importer.h"
#include "runtime/resource/asset/types/audio_clip_asset.h"

namespace dodoe {

    class AudioImporter : public AssetImporter {
    public:
        [[nodiscard]] const char* getName() const override { return "AudioImporter"; }
        Scope<Asset> import(const ImportContext& ctx) override;
    };

} // dodoe
