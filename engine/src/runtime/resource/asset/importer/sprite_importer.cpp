// do@Redlive

#include "sprite_importer.h"

#include "runtime/resource/file/file_system.h"

namespace dodoe {

    Scope<Asset> SpriteImporter::import(const ImportContext& ctx) {
        auto sprite = create_scope<SpriteAsset>();
        sprite->setFileID(ctx.source_file);
        sprite->setName(FileSystem::PathToNameNoExt(ctx.source_path));
        sprite->setTextureSource(ctx.source_file.getUUID());

        if (ctx.settings.contains("pixelsPerUnit") && ctx.settings["pixelsPerUnit"].is_number()) {
            sprite->setPixelsPerUnit(ctx.settings["pixelsPerUnit"].get<Float>());
        }
        if (ctx.settings.contains("pivot") && ctx.settings["pivot"].is_array()
            && ctx.settings["pivot"].size() >= 2) {
            sprite->setPivot({ctx.settings["pivot"][0].get<Float>(),
                              ctx.settings["pivot"][1].get<Float>()});
        }
        if (ctx.settings.contains("slice") && ctx.settings["slice"].is_array()
            && ctx.settings["slice"].size() >= 4) {
            Rect2f slice;
            slice.left = ctx.settings["slice"][0].get<Float>();
            slice.bottom = ctx.settings["slice"][1].get<Float>();
            slice.right = ctx.settings["slice"][2].get<Float>();
            slice.top = ctx.settings["slice"][3].get<Float>();
            sprite->setSlice(slice);
        }

        return sprite;
    }

} // dodoe
