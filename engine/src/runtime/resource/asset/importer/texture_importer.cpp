// do@Redlive

#include "texture_importer.h"

#include "runtime/resource/file/file_system.h"

namespace dodoe {

    Scope<Asset> TextureImporter::import(const ImportContext& ctx) {
        auto texture = create_scope<TextureAsset>();
        texture->setName(FileSystem::PathToNameNoExt(ctx.source_path));

        if (ctx.settings.contains("flipVertical") && ctx.settings["flipVertical"].is_boolean()) {
            texture->setFlipVertical(ctx.settings["flipVertical"].get<Bool>());
        }
        if (ctx.settings.contains("pixelsPerUnit") && ctx.settings["pixelsPerUnit"].is_number()) {
            texture->setPPU(ctx.settings["pixelsPerUnit"].get<Float>());
        }

        if (texture->loadFromSource(ctx.absolute_source_path)) {
            texture->setLoadState(AssetLoadState::Loaded);
        }

        return texture;
    }

} // dodoe
