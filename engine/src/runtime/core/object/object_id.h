// do@Redlive

#pragma once

#include <cstdint>
#include <functional>

#include "runtime/core/utils/uuid.h"

namespace dodoe {

    struct ObjectID {
        UUID asset_id{0};
        UInt32 local_id{0};

        [[nodiscard]] Bool isValid() const { return asset_id.isValid(); }

        Bool operator==(const ObjectID& other) const {
            return asset_id == other.asset_id && local_id == other.local_id;
        }
        Bool operator!=(const ObjectID& other) const {
            return !(*this == other);
        }
    };

    struct ObjectHandle {
        InstanceID id{0};
        UInt32 generation{0};

        [[nodiscard]] Bool isValid() const { return id != 0; }

        Bool operator==(const ObjectHandle& other) const {
            return id == other.id && generation == other.generation;
        }
        Bool operator!=(const ObjectHandle& other) const {
            return !(*this == other);
        }
    };

} // dodoe

template<>
struct std::hash<dodoe::ObjectID> {
    std::size_t operator()(const dodoe::ObjectID& id) const noexcept {
        const std::uint64_t v = static_cast<std::uint64_t>(id.asset_id) ^ (static_cast<std::uint64_t>(id.local_id) << 32);
        return static_cast<std::size_t>(v);
    }
};
