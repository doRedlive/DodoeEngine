// do@Redlive

#pragma once

#include <cstdint>
#include <functional>

#include "runtime/core/utils/uuid.h"

namespace dodoe {

    class DODOE_API FileID {
        UInt32 m_intern_id{0};

    public:
        FileID() = default;
        explicit FileID(const String& path) { *this = Make(path); }

        static FileID Make(const String& path);

        [[nodiscard]] const String& getPath() const;
        [[nodiscard]] UInt32 getInternID() const { return m_intern_id; }
        [[nodiscard]] Bool isValid() const { return m_intern_id != 0; }

        Bool operator==(const FileID& other) const { return m_intern_id == other.m_intern_id; }
        Bool operator!=(const FileID& other) const { return m_intern_id != other.m_intern_id; }
    };

} // dodoe

template<>
struct std::hash<dodoe::FileID> {
    std::size_t operator()(const dodoe::FileID& id) const noexcept {
        return static_cast<std::size_t>(id.getInternID());
    }
};
