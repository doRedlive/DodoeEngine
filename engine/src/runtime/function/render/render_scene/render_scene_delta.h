// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    enum class PrimitiveUpdateType : UInt32;
    enum class SpriteUpdateType : UInt32;
    enum class LightUpdateType : UInt32;

    using FrameNumber = UInt64;

    struct RenderSceneDelta {
        FrameNumber source_frame{0};

        UnorderedMap<UUID, PrimitiveUpdateType> primitive_updates{};
        UnorderedMap<UUID, SpriteUpdateType> sprite_updates{};
        UnorderedMap<UUID, LightUpdateType> light_updates{};

        [[nodiscard]] Bool hasAnyChange() const {
            return !primitive_updates.empty()
                || !sprite_updates.empty()
                || !light_updates.empty();
        }

        [[nodiscard]] Bool hasPrimitiveChanges() const {
            return !primitive_updates.empty();
        }

        [[nodiscard]] Bool hasSpriteChanges() const {
            return !sprite_updates.empty();
        }

        [[nodiscard]] Bool hasLightChanges() const {
            return !light_updates.empty();
        }

        void clear() {
            primitive_updates.clear();
            sprite_updates.clear();
            light_updates.clear();
        }
    };

} // namespace dodoe
