// do@Redlive

#pragma once

#include <cstdint>
#include <functional>

namespace dodoe {
    class UUID {
    public:
        UUID();
        explicit UUID(uint64_t value);

        [[nodiscard]] bool isValid() const { return m_uuid != 0; }

        bool operator==(const UUID& other) const noexcept {
            return m_uuid == other.m_uuid;
        }
        explicit operator uint64_t() const;

    private:
        uint64_t m_uuid;
    };

    using Uuid = UUID;

} // dodoe

namespace std {
    template<>
    struct hash<dodoe::Uuid> {
        std::size_t operator()(const dodoe::Uuid& uuid) const noexcept {
            return static_cast<uint64_t>(uuid);
        }
    };
    
} // std
