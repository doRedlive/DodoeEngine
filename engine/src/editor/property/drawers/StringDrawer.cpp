// do@Redlive

#include "StringDrawer.h"
#include "framework/EditorContext.h"
#include "property/FieldEditCommand.h"
#include "framework/command/CommandStack.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/json.h"

#include <QLineEdit>
#include <QSignalBlocker>

namespace cakery {

QWidget* StringDrawer::build(const PropertyContext& pc)
{
    auto* le = new QLineEdit();
    auto field = *pc.field;
    auto* str = static_cast<dodoe::String*>(field.get(pc.componentPtr));
    le->setText(QString::fromUtf8(str->c_str(), static_cast<int>(str->size())));

    QObject::connect(le, &QLineEdit::editingFinished, [pc, field, le]() mutable {
        dodoe::String oldVal = *static_cast<dodoe::String*>(field.get(pc.componentPtr));
        const std::string newText = le->text().toStdString();
        dodoe::String newVal(newText.c_str(), newText.size());
        field.set(pc.componentPtr, &newVal);
        auto cmd = MakeFieldEditCommand(pc, field.getFieldName(),
            dodoe::Json(std::string(oldVal.c_str())), dodoe::Json(newText));
        pc.ctx->commands().execute(std::move(cmd));
    });

    m_widget = le;
    return le;
}

void StringDrawer::updateValue(const PropertyContext& pc)
{
    if (!m_widget || !pc.field) return;

    auto* le = static_cast<QLineEdit*>(m_widget);
    auto* str = static_cast<dodoe::String*>(pc.field->get(pc.componentPtr));
    if (!str) return;

    const QString newText = QString::fromUtf8(str->c_str(), static_cast<int>(str->size()));
    if (le->text() != newText) {
        QSignalBlocker blocker(le);
        le->setText(newText);
    }
}

} // namespace cakery
