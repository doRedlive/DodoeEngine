// do@Redlive

#pragma once

#include "runtime/core/application.h"

extern dodoe::Application* dodoe::CreateApplication(ApplicationCommandLineArgs args);

#ifndef DODOE_BUILD_DLL
int main(int argc, char** args) {
    const auto app = dodoe::CreateApplication({argc, args});
    app->run();
    delete app;
}
#endif // DODOE_BUILD_DLL
