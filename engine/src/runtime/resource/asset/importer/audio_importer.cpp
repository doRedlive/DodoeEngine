// do@Redlive

#include "audio_importer.h"

#include "runtime/resource/file/file_system.h"

namespace dodoe {

    Scope<Asset> AudioImporter::import(const ImportContext& ctx) {
        auto audio = create_scope<AudioClipAsset>();
        audio->setName(FileSystem::PathToNameNoExt(ctx.source_path));

        if (audio->loadFromSource(ctx.absolute_source_path)) {
            audio->setLoadState(AssetLoadState::Loaded);
        }

        return audio;
    }

} // dodoe
