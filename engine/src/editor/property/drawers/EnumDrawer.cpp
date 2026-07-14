// do@Redlive

#include "EnumDrawer.h"
#include "framework/EditorContext.h"
#include "framework/command/commands/SetFieldValueCommand.h"
#include "framework/command/CommandStack.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/json.h"

#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>

namespace cakery {

QWidget* EnumDrawer::build(const PropertyContext& pc)
{
    const std::string typeName = pc.field->getFieldTypeName();
    auto* combo = new QComboBox();

    dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName(typeName);
    if (meta.isValid()) {
        dodoe::FieldAccessor* fields = nullptr;
        int count = meta.get_field_list(fields);
        for (int i = 0; i < count; ++i) {
            if (std::strcmp(fields[i].getFieldName(), "value") == 0) {
                combo->addItem(QString::fromUtf8(fields[i].getFieldName()));
            }
        }
        delete[] fields;
    }

    if (combo->count() == 0) {
        combo->addItem(QString::fromStdString(typeName));
    }

    auto field = *pc.field;
    int* val = static_cast<int*>(field.get(pc.componentPtr));
    if (val) {
        combo->setCurrentIndex(std::clamp(*val, 0, combo->count() - 1));
    }

    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [pc, field](int idx) mutable {
            int oldVal = *static_cast<int*>(field.get(pc.componentPtr));
            field.set(pc.componentPtr, &idx);
            auto cmd = std::make_unique<SetFieldValueCommand>(
                pc.entity, pc.componentName, field.getFieldName(),
                dodoe::Json(std::to_string(oldVal)), dodoe::Json(std::to_string(idx)));
            pc.ctx->commands().execute(std::move(cmd));
        });

    return combo;
}

void EnumDrawer::updateValue(const PropertyContext&) {}

} // namespace cakery
