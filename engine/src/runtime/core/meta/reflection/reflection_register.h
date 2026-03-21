//
// Created by Redlive on 2026/3/20.
//

#ifndef DODOE_REFLECTION_REGISTER_H
#define DODOE_REFLECTION_REGISTER_H

namespace dodoe {
    namespace reflection {
        class TypeMetaRegister {
        public:
            static void metaRegister();
            static void metaUnregister();
        };
    } // reflection
} // dodoe

#endif//DODOE_REFLECTION_REGISTER_H
