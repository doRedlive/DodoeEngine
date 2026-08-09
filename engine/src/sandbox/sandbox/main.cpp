// do@Redlive

#include "runtime/core/application.h"
#include "runtime/core/project/project.h"

int main(int argc, char* argv[]) {
    dodoe::ApplicationCommandLineArgs cli_args{};
    cli_args.argc = argc;
    cli_args.args = argv;

    auto project = dodoe::Project::Load("tests/Projects/OnlyOne/OnlyOne.doproj");
    if (!project) {
        DO_ERROR("Failed to load project");
        return -1;
    }

    auto* app = dodoe::CreateApplication(cli_args);
    app->run();
    delete app;

    return 0;
}
