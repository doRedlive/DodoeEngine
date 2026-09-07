// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    class RenderId {
    public:
        RenderId() = default;
        explicit RenderId(const UInt64 value) : m_value(value) { }

        [[nodiscard]] UInt64 value() const { return m_value; }
        [[nodiscard]] Bool isValid() const { return m_value != 0; }

        bool operator==(const RenderId& other) const noexcept { return m_value == other.m_value; }
        bool operator!=(const RenderId& other) const noexcept { return m_value != other.m_value; }

    private:
        UInt64 m_value{0};
    };

} // dodoe

template<>
struct std::hash<dodoe::RenderId> {
    std::size_t operator()(const dodoe::RenderId& id) const noexcept {
        return id.value();
    }
};
