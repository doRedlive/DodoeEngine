// do@Redlive

#include "runtime/core/application.h"
#include "runtime/core/debug/instrumentor.h"
#include "runtime/core/project/project.h"

int main(int argc, char* argv[]) {
    DO_PROFILE_BEGIN_SESSION("DodoeSandbox", "DodoeProfile.json");
    DO_PROFILE_THREAD_NAME("MainThread");
    int exit_code = 0;
    {
        DO_PROFILE_SCOPE_CATEGORY("Sandbox::main", "startup");

        dodoe::ApplicationCommandLineArgs cli_args{};
        cli_args.argc = argc;
        cli_args.args = argv;

        auto project = dodoe::Project::Load("tests/Projects/OnlyOne/OnlyOne.doproj");
        if (!project) {
            DO_ERROR("Failed to load project.");
            exit_code = -1;
        } else {
            auto* app = dodoe::CreateApplication(cli_args);
            app->run();
            delete app;
        }
    }
    DO_PROFILE_END_SESSION();

    return exit_code;
}
