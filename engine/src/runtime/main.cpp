//
// Created by GreenMuffin on 2025/10/26.
//

#include "core/application.h"

int main(int argc, char** args) {

    const auto app = dodoe::create_application({argc, args});
    app->run();
    delete app;
    
}