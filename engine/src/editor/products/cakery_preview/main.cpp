// do@Redlive

#include "adapters/null/NullEditorBackend.h"
#include "cakery/app/EditorApplication.h"

#include <memory>

int main(int argc, char* argv[])
{
    cakery::EditorApplication app(argc, argv, QStringLiteral("CakeryPreview"),
                                  std::make_unique<cakery::NullEditorBackend>());
    return app.run();
}
