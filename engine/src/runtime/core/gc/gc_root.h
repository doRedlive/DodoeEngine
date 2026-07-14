// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/core/object/object.h"

#include <mutex>

namespace dodoe {

    class GCRootRegistry {
        DynamicArray<Object*> m_roots{};
        std::mutex m_mutex{};

        GCRootRegistry() = default;

    public:
        static GCRootRegistry& instance();

        void registerRoot(Object* obj);
        void unregisterRoot(InstanceID id);
        void collectRoots(DynamicArray<Object*>& out_roots);
    };

} // namespace dodoe
