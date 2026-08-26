// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    using AudioSourceId = UInt32;
    constexpr AudioSourceId kInvalidAudioSourceId = 0;

    enum class AudioPlayState : UInt8 {
        Stopped,
        Playing,
        Paused
    };

    struct AudioSystemCreateInfo {
        Float master_volume{1.0f};
    };

    struct AudioBackendInitInfo {
        Float master_volume{1.0f};
    };

} // dodoe
