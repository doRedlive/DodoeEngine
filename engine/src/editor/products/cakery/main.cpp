// do@Redlive

#include "adapters/runtime/RuntimeEditorBackend.h"
#include "cakery/app/EditorApplication.h"

#include <memory>

int main(int argc, char* argv[])
{
    cakery::EditorApplication app(argc, argv, QStringLiteral("Cakery"),
                                  std::make_unique<cakery::RuntimeEditorBackend>());
    return app.run();
}
