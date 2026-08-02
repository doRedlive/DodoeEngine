// do@Redlive

#pragma once

#include <cstdint>
#include <functional>

#include "runtime/core/container/containers.h"

namespace dodoe {
    class DODOE_API UUID {
    public:
        UUID();
        explicit UUID(uint64_t value);

        static UUID Generate();
        static UUID FromString(const String& str);

        [[nodiscard]] bool isValid() const { return m_uuid != 0; }

        bool operator==(const UUID& other) const noexcept {
            return m_uuid == other.m_uuid;
        }
        explicit operator uint64_t() const;

    private:
        uint64_t m_uuid;
    };

} // dodoe

template<>
struct std::hash<dodoe::UUID> {
    std::size_t operator()(const dodoe::UUID& uuid) const noexcept {
        return static_cast<uint64_t>(uuid);
    }
};
