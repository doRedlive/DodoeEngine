// do@Redlive

#pragma once

#include "runtime/core/application.h"
#include "runtime/core/debug/instrumentor.h"

extern dodoe::Application* dodoe::CreateApplication(ApplicationCommandLineArgs args);

int main(int argc, char** args) {
    DO_PROFILE_BEGIN_SESSION("StartUp", "DodoeProfile-StartUp.json");
    const auto app = dodoe::CreateApplication({argc, args});
    DO_PROFILE_END_SESSION();

    DO_PROFILE_BEGIN_SESSION("Runtime", "DodoeProfile-Runtime.json");
    app->run();
    DO_PROFILE_END_SESSION();

    DO_PROFILE_BEGIN_SESSION("Shutdown", "DodoeProfile-Shutdown.json");
    delete app;
    DO_PROFILE_END_SESSION();
}
