//
// Created by GreenMuffin on 2025/10/26.
//

#include "runtime/core/application.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/meta/reflection/reflection_register.h"
#include <iostream>
#include <string>

static bool has_arg(int argc, char** argv, const char* target) {
    if (!target) {
        return false;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] && std::string(argv[i]) == target) {
            return true;
        }
    }
    return false;
}

static int run_reflection_test() {
    dodoe::TypeMetaRegister::meta_register();

    std::cout << std::endl << "================ REFLECTION TEST ================" << std::endl;
    dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName("SampleClass");
    if (!meta.isValid()) {
        std::cout << "Failed to find SampleClass in reflection system!" << std::endl;
        dodoe::TypeMetaRegister::meta_unregister();
        return 1;
    }

    std::cout << "Successfully found class: " << meta.get_type_name() << std::endl;

    dodoe::FieldAccessor* fields = nullptr;
    const int field_count = meta.get_field_list(fields);
    std::cout << "Class has " << field_count << " fields:" << std::endl;
    for (int i = 0; i < field_count; ++i) {
        std::cout << "  - Field: " << fields[i].getFieldName()
                  << " | Type: " << fields[i].getFieldTypeName()
                  << " | IsArray: " << (fields[i].is_array_type() ? "true" : "false") << std::endl;
    }

    delete[] fields;

    std::cout << "=================================================" << std::endl << std::endl;

    dodoe::TypeMetaRegister::meta_unregister();
    return 0;
}

int main(int argc, char** args) {
    if (has_arg(argc, args, "--reflection-test")) {
        return run_reflection_test();
    }

    const auto app = dodoe::create_application({argc, args});
    app->run();
    delete app;
    return 0;
}
