#ifndef DODOE_SAMPLE_CLASS_H
#define DODOE_SAMPLE_CLASS_H

#include "runtime/core/meta/reflection/reflection.h"
#include <string>

REFLECTION_TYPE(SampleClass)

namespace dodoe {
    CLASS(SampleClass, WhiteListFields)
    {
        REFLECTION_BODY(SampleClass)
    public:
        SampleClass() = default;
        ~SampleClass() = default;

        META(Enable)
        int sample_value = 42;

        META(Enable)
        std::string sample_name = "Dodoe Engine";

        void print_info() const {
        }
    };

} // namespace dodoe

#endif // DODOE_SAMPLE_CLASS_H
