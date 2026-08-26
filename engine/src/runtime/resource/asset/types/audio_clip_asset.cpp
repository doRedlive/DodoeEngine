// do@Redlive

#include "audio_clip_asset.h"

namespace dodoe {

    Bool AudioClipAsset::loadFromSource(const String& absolute_source_path) {
        unloadRuntime();
        m_clip = create_scope<AudioClip>(getObjectID());
        return m_clip->loadFromFile(absolute_source_path);
    }

    void AudioClipAsset::unloadRuntime() {
        m_clip.reset();
    }

} // dodoe
