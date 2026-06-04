// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/core/utils/uuid.h"

namespace dodoe {

    class FileID {
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

        [[nodiscard]] Bool isValid() const { return m_id != 0; }

        Bool operator==(const FileID& other) const { return m_id == other.m_id; }
    };

} // dodoe

namespace std {
    template<>
    struct hash<dodoe::FileID> {
        dodoe::Size_t operator()(const dodoe::FileID& id) const noexcept {
            return static_cast<dodoe::Size_t>(id.getID());
        }
    };
} // std
