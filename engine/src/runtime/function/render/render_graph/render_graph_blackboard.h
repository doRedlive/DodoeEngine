// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    class RenderGraphBlackboard {
        UnorderedMap<Size_t, Ref<void>> m_values{};

    public:
        RenderGraphBlackboard() = default;

        template <typename TTag, typename TValue>
        void set(const TValue& value) {
            m_values[typeid(TTag).hash_code()] = create_ref<TValue>(value);
        }

        template <typename TTag, typename TValue>
        [[nodiscard]] TValue* get() {
            auto it = m_values.find(typeid(TTag).hash_code());
            if (it == m_values.end()) {
                return nullptr;
            }
            return static_cast<TValue*>(it->second.get());
        }

        template <typename TTag, typename TValue>
        [[nodiscard]] const TValue* get() const {
            auto it = m_values.find(typeid(TTag).hash_code());
            if (it == m_values.end()) {
                return nullptr;
            }
            return static_cast<const TValue*>(it->second.get());
        }

        void reset() { m_values.clear(); }
    };

} // dodoe

