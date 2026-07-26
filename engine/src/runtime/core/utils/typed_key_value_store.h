// do@Redlive

#pragma once

#include "dopch.h"

#include <typeindex>

namespace dodoe {

    class TypedKeyValueStore {
        UnorderedMap<std::type_index, Ref<void>> m_values{};

    public:
        template <typename TKey, typename TValue>
        void set(const TValue& value) {
            m_values[std::type_index(typeid(TKey))] = create_ref<TValue>(value);
        }

        template <typename TKey, typename TValue>
        [[nodiscard]] TValue* get() {
            auto it = m_values.find(std::type_index(typeid(TKey)));
            return it != m_values.end() ? static_cast<TValue*>(it->second.get()) : nullptr;
        }

        template <typename TKey, typename TValue>
        [[nodiscard]] const TValue* get() const {
            auto it = m_values.find(std::type_index(typeid(TKey)));
            return it != m_values.end() ? static_cast<const TValue*>(it->second.get()) : nullptr;
        }

        void clear() { m_values.clear(); }
    };

} // namespace dodoe
