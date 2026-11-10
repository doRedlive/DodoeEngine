// do@Redlive

#include "dopch.h"

#include "app/EditorApplication.h"

int main(int argc, char* argv[])
{
    cakery::EditorApplication app(argc, argv, QStringLiteral("Cakery"));
    return app.run();
}
