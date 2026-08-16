// do@Redlive

#include "ScalarDrawer.h"
#include "framework/EditorContext.h"
#include "property/FieldEditCommand.h"
#include "framework/command/CommandStack.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/json.h"

#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QSignalBlocker>
#include <cmath>
#include <string>

namespace cakery {

QWidget* ScalarDrawer::build(const PropertyContext& pc)
{
    const dodoe::FieldType ft = pc.field->getFieldType();
    void* instance = pc.componentPtr;
    auto field = *pc.field;

    if (ft == dodoe::FieldType::Bool) {
        auto* cb = new QCheckBox();
        bool* val = static_cast<bool*>(field.get(instance));
        cb->setChecked(*val);

        QObject::connect(cb, &QCheckBox::toggled, [pc, field](bool v) mutable {
            bool bv = v;
            dodoe::Json oldVal = *static_cast<bool*>(field.get(pc.componentPtr));
            dodoe::Json newVal = bv;
            field.set(pc.componentPtr, &bv);
            auto cmd = MakeFieldEditCommand(pc, field.getFieldName(), std::move(oldVal), std::move(newVal));
            pc.ctx->commands().execute(std::move(cmd));
        });

        m_widget = cb;
        return cb;
    }

    auto* sb = new QDoubleSpinBox();
    sb->setRange(-99999.0, 99999.0);
    sb->setDecimals(3);
    sb->setSingleStep(0.1);

    float rangeMin = -99999.0f, rangeMax = 99999.0f;
    if (field.attributeRange(rangeMin, rangeMax)) {
        sb->setRange(rangeMin, rangeMax);
        sb->setSingleStep((rangeMax - rangeMin) / 100.0);
    }
    if (field.isReadOnly()) {
        sb->setEnabled(false);
    }

    auto setValue = [&](QDoubleSpinBox* spin) {
        switch (ft) {
        case dodoe::FieldType::F32:
            spin->setValue(static_cast<double>(*static_cast<float*>(field.get(instance))));
            break;
        case dodoe::FieldType::F64:
            spin->setValue(*static_cast<double*>(field.get(instance)));
            break;
        case dodoe::FieldType::I32:
            spin->setDecimals(0);
            spin->setSingleStep(1.0);
            spin->setValue(static_cast<double>(*static_cast<int*>(field.get(instance))));
            break;
        case dodoe::FieldType::U32:
            spin->setDecimals(0);
            spin->setSingleStep(1.0);
            spin->setRange(0.0, 99999.0);
            spin->setValue(static_cast<double>(*static_cast<unsigned int*>(field.get(instance))));
            break;
        default:
            break;
        }
    };

    setValue(sb);

    QObject::connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        [pc, field, ft](double v) mutable {
            switch (ft) {
            case dodoe::FieldType::F32: {
                float fv = static_cast<float>(v);
                float oldFv = *static_cast<float*>(field.get(pc.componentPtr));
                dodoe::Json oldVal = oldFv;
                dodoe::Json newVal = fv;
                field.set(pc.componentPtr, &fv);
                auto cmd = MakeFieldEditCommand(pc, field.getFieldName(), std::move(oldVal), std::move(newVal));
                pc.ctx->commands().execute(std::move(cmd));
                break;
            }
            case dodoe::FieldType::F64: {
                double oldV = *static_cast<double*>(field.get(pc.componentPtr));
                dodoe::Json oldVal = oldV;
                dodoe::Json newVal = v;
                field.set(pc.componentPtr, &v);
                auto cmd = MakeFieldEditCommand(pc, field.getFieldName(), std::move(oldVal), std::move(newVal));
                pc.ctx->commands().execute(std::move(cmd));
                break;
            }
            case dodoe::FieldType::I32: {
                int iv = static_cast<int>(std::round(v));
                int oldIv = *static_cast<int*>(field.get(pc.componentPtr));
                dodoe::Json oldVal = oldIv;
                dodoe::Json newVal = iv;
                field.set(pc.componentPtr, &iv);
                auto cmd = MakeFieldEditCommand(pc, field.getFieldName(), std::move(oldVal), std::move(newVal));
                pc.ctx->commands().execute(std::move(cmd));
                break;
            }
            case dodoe::FieldType::U32: {
                unsigned int uv = static_cast<unsigned int>(std::round(v));
                unsigned int oldUv = *static_cast<unsigned int*>(field.get(pc.componentPtr));
                dodoe::Json oldVal = oldUv;
                dodoe::Json newVal = uv;
                field.set(pc.componentPtr, &uv);
                auto cmd = MakeFieldEditCommand(pc, field.getFieldName(), std::move(oldVal), std::move(newVal));
                pc.ctx->commands().execute(std::move(cmd));
                break;
            }
            default:
                break;
            }
        });

    m_widget = sb;
    return sb;
}

void ScalarDrawer::updateValue(const PropertyContext& pc)
{
    if (!m_widget || !pc.field) return;

    const dodoe::FieldType ft = pc.field->getFieldType();
    if (ft == dodoe::FieldType::Bool) {
        auto* cb = static_cast<QCheckBox*>(m_widget);
        bool* v = static_cast<bool*>(pc.field->get(pc.componentPtr));
        if (v) {
            QSignalBlocker blocker(cb);
            cb->setChecked(*v);
        }
        return;
    }

    auto* sb = static_cast<QDoubleSpinBox*>(m_widget);
    QSignalBlocker blocker(sb);
    switch (ft) {
    case dodoe::FieldType::F32:
        sb->setValue(static_cast<double>(*static_cast<float*>(pc.field->get(pc.componentPtr))));
        break;
    case dodoe::FieldType::F64:
        sb->setValue(*static_cast<double*>(pc.field->get(pc.componentPtr)));
        break;
    case dodoe::FieldType::I32:
        sb->setValue(static_cast<double>(*static_cast<int*>(pc.field->get(pc.componentPtr))));
        break;
    case dodoe::FieldType::U32:
        sb->setValue(static_cast<double>(*static_cast<unsigned int*>(pc.field->get(pc.componentPtr))));
        break;
    default:
        break;
    }
}

} // namespace cakery
