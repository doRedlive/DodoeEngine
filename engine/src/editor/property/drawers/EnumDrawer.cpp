// do@Redlive

#include "EnumDrawer.h"
#include "framework/EditorContext.h"
#include "framework/command/commands/SetFieldValueCommand.h"
#include "framework/command/CommandStack.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/json.h"

#include <QComboBox>
#include <QSignalBlocker>

namespace cakery {

QWidget* EnumDrawer::build(const PropertyContext& pc)
{
    auto* combo = new QComboBox();
    auto field = *pc.field;

    dodoe::EnumValueList values;
    if (!field.enumValues(values) || values.empty()) {
        combo->addItem(QString::fromStdString(pc.field->getFieldTypeName()));
    }
    else {
        for (const auto& [name, value] : values) {
            combo->addItem(QString::fromUtf8(name), value);
        }
    }

    int* val = static_cast<int*>(field.get(pc.componentPtr));
    if (val) {
        int idx = combo->findData(*val);
        combo->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [pc, field, combo](int idx) mutable {
            int oldVal = *static_cast<int*>(field.get(pc.componentPtr));
            int newVal = combo->itemData(idx).toInt();
            field.set(pc.componentPtr, &newVal);
            auto cmd = std::make_unique<SetFieldValueCommand>(
                pc.entity, pc.componentName, field.getFieldName(),
                dodoe::Json(std::to_string(oldVal)), dodoe::Json(std::to_string(newVal)));
            pc.ctx->commands().execute(std::move(cmd));
        });

    m_widget = combo;
    return combo;
}

void EnumDrawer::updateValue(const PropertyContext& pc)
{
    if (!m_widget || !pc.field) return;

    auto* combo = static_cast<QComboBox*>(m_widget);
    int* val = static_cast<int*>(pc.field->get(pc.componentPtr));
    if (!val) return;

    QSignalBlocker blocker(combo);
    int idx = combo->findData(*val);
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
}

} // namespace cakery
