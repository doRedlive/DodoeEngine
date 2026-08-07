// do@Redlive

#include "model_importer.h"

#include "runtime/resource/file/file_system.h"

namespace dodoe {

    Scope<Asset> ModelImporter::import(const ImportContext& ctx) {
        auto mesh = create_scope<MeshAsset>();
        mesh->setFileID(ctx.source_file);
        mesh->setName(FileSystem::PathToNameNoExt(ctx.source_path));
        return mesh;
    }

} // dodoe
