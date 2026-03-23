#include "reflection_register.h"

#include "runtime/core/meta/json.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/meta/serializer/serializer.h"

#include "_generated/reflection/all_reflection.h"
#include "_generated/serializer/all_serializer.ipp"

namespace dodoe {
    namespace reflection {
        void TypeMetaRegister::meta_register() {
        }

        void TypeMetaRegister::meta_unregister() {
            TypeMetaRegisterInterface::unregister_all();
        }
    } // reflection
} // dodoe
