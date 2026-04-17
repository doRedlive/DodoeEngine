//
// Created by Redlive on 2026/3/20.
//

#ifndef DODOE_REFLECTION_REGISTER_H
#define DODOE_REFLECTION_REGISTER_H

#include "dopch.h"

namespace dodoe {
    class TypeMetaRegister {
    public:
        static void meta_register();
        static void meta_unregister();
    };
} // dodoe

#endif//DODOE_REFLECTION_REGISTER_H
