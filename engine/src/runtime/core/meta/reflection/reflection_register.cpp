// do@Redlive

#include "reflection_register.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/meta/serializer/serializer.h"

#include "_generated/reflection/all_reflection.h"
#include "_generated/serializer/all_serializer.ipp"

namespace dodoe {

    void TypeMetaRegister::MetaRegister() {
        TypeFieldReflectionOperator::RegisterAllReflection();
    }

    void TypeMetaRegister::MetaUnregister() {
        TypeMetaRegisterInterface::unregister_all();
    }

} // dodoe
