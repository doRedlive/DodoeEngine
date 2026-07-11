// do@Redlive

#include "StringDrawer.h"
#include "framework/EditorContext.h"
#include "framework/command/commands/SetFieldValueCommand.h"
#include "framework/command/CommandStack.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/json.h"

#include <QLineEdit>

namespace cakery {

QWidget* StringDrawer::build(const PropertyContext& pc)
{
    auto* le = new QLineEdit();
    auto field = *pc.field;
    std::string* str = static_cast<std::string*>(field.get(pc.componentPtr));
    le->setText(QString::fromStdString(*str));

    QObject::connect(le, &QLineEdit::editingFinished, [pc, field, le]() mutable {
        std::string oldVal = *static_cast<std::string*>(field.get(pc.componentPtr));
        std::string newVal = le->text().toStdString();
        field.set(pc.componentPtr, &newVal);
        auto cmd = std::make_unique<SetFieldValueCommand>(
            pc.entity, pc.componentName, field.getFieldName(),
            dodoe::Json(oldVal), dodoe::Json(newVal));
        pc.ctx->commands().execute(std::move(cmd));
    });

    return le;
}

void StringDrawer::updateValue(const PropertyContext& /*pc*/)
{
}

} // namespace cakery
