// do@Redlive

#include "tiled_map_importer.h"

#include "runtime/resource/file/file_system.h"

namespace dodoe {

    Scope<Asset> TiledMapImporter::import(const ImportContext& ctx) {
        auto map = create_scope<TiledMapAsset>();
        map->setName(FileSystem::PathToNameNoExt(ctx.source_path));

        if (map->loadFromSource(ctx.absolute_source_path)) {
            map->setLoadState(AssetLoadState::Loaded);
        }

        return map;
    }

} // dodoe
