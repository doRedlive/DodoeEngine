// do@Redlive

#include "runtime/core/application.h"
#include "runtime/core/project/project.h"

int main(int argc, char* argv[]) {
    dodoe::ApplicationCommandLineArgs cli_args{};
    cli_args.argc = argc;
    cli_args.args = argv;

    auto* app = dodoe::CreateApplication(cli_args);

    // Load the test project (required before starting runtime)
    auto project = dodoe::Project::Load("tests/Projects/OnlyOne/OnlyOne.doproj");
    if (!project) {
        DO_ERROR("Failed to load project");
        delete app;
        return -1;
    }

    app->run();
    delete app;

    return 0;
}
