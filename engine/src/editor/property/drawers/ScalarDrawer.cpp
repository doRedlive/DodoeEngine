// do@Redlive

#include "ScalarDrawer.h"
#include "framework/EditorContext.h"
#include "framework/command/commands/SetFieldValueCommand.h"
#include "framework/command/CommandStack.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/json.h"

#include <QDoubleSpinBox>
#include <QCheckBox>
#include <cmath>
#include <string>

namespace cakery {

QWidget* ScalarDrawer::build(const PropertyContext& pc)
{
    const std::string typeName = pc.field->getFieldTypeName();
    void* instance = pc.componentPtr;
    auto field = *pc.field;

    if (typeName == "bool") {
        auto* cb = new QCheckBox();
        bool* val = static_cast<bool*>(field.get(instance));
        cb->setChecked(*val);

        QObject::connect(cb, &QCheckBox::toggled, [pc, field](bool v) mutable {
            bool bv = v;
            dodoe::Json oldVal = *static_cast<bool*>(field.get(pc.componentPtr)) ? "true" : "false";
            dodoe::Json newVal = bv ? "true" : "false";
            field.set(pc.componentPtr, &bv);
            auto cmd = std::make_unique<SetFieldValueCommand>(
                pc.entity, pc.componentName, field.getFieldName(), oldVal, newVal);
            pc.ctx->commands().execute(std::move(cmd));
        });

        return cb;
    }

    auto* sb = new QDoubleSpinBox();
    sb->setRange(-99999.0, 99999.0);
    sb->setDecimals(3);
    sb->setSingleStep(0.1);

    auto setValue = [&](QDoubleSpinBox* spin) {
        if (typeName == "float") {
            spin->setValue(static_cast<double>(*static_cast<float*>(field.get(instance))));
        } else if (typeName == "double") {
            spin->setValue(*static_cast<double*>(field.get(instance)));
        } else if (typeName == "int" || typeName == "int32_t") {
            spin->setDecimals(0);
            spin->setSingleStep(1.0);
            spin->setValue(static_cast<double>(*static_cast<int*>(field.get(instance))));
        } else if (typeName == "uint32_t") {
            spin->setDecimals(0);
            spin->setSingleStep(1.0);
            spin->setRange(0.0, 99999.0);
            spin->setValue(static_cast<double>(*static_cast<unsigned int*>(field.get(instance))));
        }
    };

    setValue(sb);

    QObject::connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        [pc, field, typeName](double v) mutable {
            if (typeName == "float") {
                float fv = static_cast<float>(v);
                float oldFv = *static_cast<float*>(field.get(pc.componentPtr));
                dodoe::Json oldVal = std::to_string(oldFv);
                dodoe::Json newVal = std::to_string(fv);
                field.set(pc.componentPtr, &fv);
                auto cmd = std::make_unique<SetFieldValueCommand>(
                    pc.entity, pc.componentName, field.getFieldName(), oldVal, newVal);
                pc.ctx->commands().execute(std::move(cmd));
            } else if (typeName == "double") {
                double oldV = *static_cast<double*>(field.get(pc.componentPtr));
                dodoe::Json oldVal = std::to_string(oldV);
                dodoe::Json newVal = std::to_string(v);
                field.set(pc.componentPtr, &v);
                auto cmd = std::make_unique<SetFieldValueCommand>(
                    pc.entity, pc.componentName, field.getFieldName(), oldVal, newVal);
                pc.ctx->commands().execute(std::move(cmd));
            } else {
                int iv = static_cast<int>(std::round(v));
                field.set(pc.componentPtr, &iv);
            }
        });

    return sb;
}

void ScalarDrawer::updateValue(const PropertyContext& pc)
{
    (void)pc;
}

} // namespace cakery
