// do@Redlive

#pragma once

#include <cstdint>
#include <functional>

#include "runtime/core/utils/uuid.h"

namespace dodoe {

    class DODOE_API FileID {
        String m_path{};
        UUID m_uuid{};
        UInt64 m_id{0};

        [[nodiscard]] static UInt64 computeID(const String& path, const UUID& uuid);

    public:
        FileID() = default;
        explicit FileID(const String& path);
        FileID(const String& path, const UUID& uuid);

        [[nodiscard]] const String& getPath() const { return m_path; }
        [[nodiscard]] const UUID& getUUID() const { return m_uuid; }
        [[nodiscard]] UInt64 getID() const { return m_id; }

        [[nodiscard]] Bool isValid() const { return !m_path.empty() && m_id != 0; }

        Bool operator==(const FileID& other) const { return m_id == other.m_id; }
    };

} // dodoe

template<>
struct std::hash<dodoe::FileID> {
    std::size_t operator()(const dodoe::FileID& id) const noexcept {
        return static_cast<std::size_t>(id.getID());
    }
};
