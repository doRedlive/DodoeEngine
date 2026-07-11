// do@Redlive

#include "EnumDrawer.h"
#include "framework/EditorContext.h"
#include "framework/command/commands/SetFieldValueCommand.h"
#include "framework/command/CommandStack.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/json.h"

#include <QComboBox>

namespace cakery {

QWidget* EnumDrawer::build(const PropertyContext& pc)
{
    const std::string typeName = pc.field->getFieldTypeName();
    auto* combo = new QComboBox();

    if (typeName == "CameraType" || typeName == "dodoe::CameraType") {
        combo->addItems({"None", "Perspective", "Orthographic"});
    } else {
        combo->addItem("Unknown");
    }

    auto field = *pc.field;
    int* val = static_cast<int*>(field.get(pc.componentPtr));
    combo->setCurrentIndex(*val);

    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [pc, field](int idx) mutable {
            int oldVal = *static_cast<int*>(field.get(pc.componentPtr));
            int newVal = idx;
            field.set(pc.componentPtr, &newVal);
            auto cmd = std::make_unique<SetFieldValueCommand>(
                pc.entity, pc.componentName, field.getFieldName(),
                dodoe::Json(std::to_string(oldVal)), dodoe::Json(std::to_string(newVal)));
            pc.ctx->commands().execute(std::move(cmd));
        });

    return combo;
}

void EnumDrawer::updateValue(const PropertyContext& /*pc*/) {}

} // namespace cakery
