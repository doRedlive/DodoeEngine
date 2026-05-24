// do@Redlive

#include "reflection_register.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/meta/serializer/serializer.h"

#include "_generated/reflection/all_reflection.h"
#include "_generated/serializer/all_serializer.ipp"

namespace dodoe {

    void TypeMetaRegister::meta_register() {
        TypeFieldReflectionOperator::RegisterAllReflection();
    }

    void TypeMetaRegister::meta_unregister() {
        TypeMetaRegisterInterface::unregister_all();
    }

} // dodoe
