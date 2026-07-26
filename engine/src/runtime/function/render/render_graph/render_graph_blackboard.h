// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/core/utils/typed_key_value_store.h"

namespace dodoe {

    class RenderGraphBlackboard {
        TypedKeyValueStore m_values{};

    public:
        RenderGraphBlackboard() = default;

        template <typename TKey>
        void set(const typename TKey::Value& value) {
            m_values.set<TKey>(value);
        }

        template <typename TKey>
        [[nodiscard]] typename TKey::Value* get() {
            return m_values.get<TKey, typename TKey::Value>();
        }

        template <typename TKey>
        [[nodiscard]] const typename TKey::Value* get() const {
            return m_values.get<TKey, typename TKey::Value>();
        }

        void reset() { m_values.clear(); }
    };

} // dodoe
