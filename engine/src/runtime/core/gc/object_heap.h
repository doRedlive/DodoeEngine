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
            auto* obj = new (mem) T(std::forward<Args>(args)...);
            if (obj->getInstanceID()) {
                Object::AllocateInstanceID(obj);
            }
            return obj;
        }

        static void Destroy(Object* obj) {
            if (!obj) return;
            obj->~Object();
            Memory::Deallocate(obj, sizeof(*obj), AllocCategory::Object);
        }
    };

} // namespace dodoe
