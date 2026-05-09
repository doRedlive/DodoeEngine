// do@Redlive

#include "asset.h"

namespace dodoe {

    std::string AssetType2String(AssetType type) {
        switch (type) {
            case AssetType::None:    return "AssetType::None";
            case AssetType::Scene:   return "AssetType::Scene";
            case AssetType::Texture: return "AssetType::Texture";
            case AssetType::Model:   return "AssetType::Model";
            case AssetType::Shader:  return "AssetType::Shader";
        }
        return "AssetType::<Invalid>";
    }

} // dodoe