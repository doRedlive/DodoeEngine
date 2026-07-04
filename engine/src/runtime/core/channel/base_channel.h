// do@Redlive

#pragma once

#include "dopch.h"

#include <any>

namespace dodoe {

    template<typename T>
    struct DataSlot {
        T data{};
    };

    template<typename... Slots>
    struct DataChannel : DataSlot<Slots>... {
        static_assert(sizeof...(Slots) > 0, "DataChannel requires at least one slot type");

        template<typename T>
        [[nodiscard]] T& get() { return static_cast<DataSlot<T>&>(*this).data; }

        template<typename T>
        [[nodiscard]] const T& get() const { return static_cast<const DataSlot<T>&>(*this).data; }
    };

    class DynamicDataChannel {
        UnorderedMap<Size_t, std::any> m_slots;

    public:
        template<typename T>
        [[nodiscard]] T& slot() {
            const auto key = typeid(T).hash_code();
            auto it = m_slots.find(key);
            if (it == m_slots.end()) {
                auto [ins, _] = m_slots.emplace(key, T{});
                return std::any_cast<T&>(ins->second);
            }
            return std::any_cast<T&>(it->second);
        }
    };

} // dodoe
