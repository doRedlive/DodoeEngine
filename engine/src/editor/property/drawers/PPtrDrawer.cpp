// do@Redlive

#include "PPtrDrawer.h"
#include "runtime/core/meta/reflection/reflection.h"

#include "runtime/core/object/pptr.h"
#include "runtime/resource/file/file_id.h"

#include <QLineEdit>

namespace cakery {

QWidget* PPtrDrawer::build(const PropertyContext& pc)
{
    auto* edit = new QLineEdit();
    edit->setReadOnly(true);
    edit->setPlaceholderText("None");

    const char* typeName = pc.field->getFieldTypeName();
    if (!typeName) return edit;

    void* ptr = pc.field->get(pc.componentPtr);
    if (ptr) {
        auto* fileId = static_cast<dodoe::FileID*>(ptr);
        if (fileId->isValid()) {
            edit->setText(QString::fromStdString(fileId->getPath()));
        }
    }

    return edit;
}

void PPtrDrawer::updateValue(const PropertyContext& /*pc*/) {}

} // namespace cakery
