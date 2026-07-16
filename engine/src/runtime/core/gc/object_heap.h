// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/core/memory/memory.h"
#include "runtime/core/object/object.h"

namespace dodoe {

    class ObjectHeap {
    public:
        template <typename T, typename... Args>
        static T* Construct(AllocCategory cat, Args&&... args) {
            void* mem = Memory::Allocate(sizeof(T), alignof(T), cat, typeid(T).name());
            return new (mem) T(std::forward<Args>(args)...);
        }

        static void Destroy(Object* obj) {
            if (!obj) return;
            obj->~Object();
            Memory::DeallocatePersistent(obj, sizeof(*obj), AllocTag::Object);
        }
    };

} // namespace dodoe
