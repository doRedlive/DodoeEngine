// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/core/base.h"

namespace dodoe {

    class TraceVisitor {
        UnorderedSet<InstanceID> m_marked{};

    public:
        TraceVisitor() = default;

        void mark(InstanceID id) {
            if (id != 0) {
                m_marked.insert(id);
            }
        }

        [[nodiscard]] Bool isMarked(InstanceID id) const {
            return m_marked.find(id) != m_marked.end();
        }

        [[nodiscard]] const UnorderedSet<InstanceID>& getMarkedSet() const {
            return m_marked;
        }

        void clear() {
            m_marked.clear();
        }
    };

} // namespace dodoe
