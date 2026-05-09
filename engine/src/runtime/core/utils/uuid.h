//
// Created by GreenMuffin on 2026/1/22.
//

#pragma once

#include <cstdint>
#include <functional>

namespace dodoe {
    class UUID {
    public:
        UUID();
        explicit UUID(uint64_t value);

        [[nodiscard]] bool isValid() const { return uuid_ != 0; }

        bool operator==(const UUID& other) const noexcept {
            return uuid_ == other.uuid_;
        }
        explicit operator uint64_t() const;

    private:
        uint64_t uuid_;
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
