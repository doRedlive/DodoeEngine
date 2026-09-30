// do@Redlive

#pragma once

#include "dopch.h"

#ifdef DODOE_EDITOR_ENABLED

#include "base_channel.h"

namespace dodoe {

    struct EditorPickRequest {
        Int32 x{-1};
        Int32 y{-1};
        UInt64 sequence{0};
    };

    struct EditorPickResult {
        UInt64 sequence{0};
        UInt64 entity_uuid{0};
        Bool handled{false};
    };

    struct PickChannelData {
        EditorPickRequest request;
        EditorPickResult result;
    };

    using PickChannel = DataChannel<PickChannelData>;

    DODOE_API PickChannel& GetPickChannel();

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
