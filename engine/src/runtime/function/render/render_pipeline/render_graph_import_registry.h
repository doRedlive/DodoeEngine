// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/typed_key_value_store.h"

namespace dodoe {

    class RenderGraphImportRegistry {
        TypedKeyValueStore m_values{};
        Bool m_frozen{false};

    public:
        template <typename TKey>
        void publish(const typename TKey::Value& value) {
            DO_ASSERT(!m_frozen, "RenderGraphImportRegistry cannot publish after freeze");
            DO_ASSERT(find<TKey>() == nullptr, "RenderGraphImportRegistry key was already published");
            m_values.set<TKey>(value);
        }

        template <typename TKey>
        [[nodiscard]] const typename TKey::Value* find() const {
            return m_values.get<TKey, typename TKey::Value>();
        }

        template <typename TKey>
        [[nodiscard]] typename TKey::Value require() const {
            const auto* value = find<TKey>();
            if (!value) {
                DO_ASSERT(false, "RenderGraphImportRegistry required import is missing");
                return {};
            }
            return *value;
        }

        void freeze() { m_frozen = true; }

        void reset() {
            m_values.clear();
            m_frozen = false;
        }
    };

} // namespace dodoe
