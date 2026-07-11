// do@Redlive

#include "VectorDrawer.h"
#include "framework/EditorContext.h"
#include "framework/command/commands/SetFieldValueCommand.h"
#include "framework/command/CommandStack.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/json.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <string>

namespace cakery {

namespace {

QDoubleSpinBox* makeSpinBox(QWidget* parent, double val, double min, double max, int decimals, double step)
{
    auto* sb = new QDoubleSpinBox(parent);
    sb->setRange(min, max);
    sb->setValue(val);
    sb->setDecimals(decimals);
    sb->setSingleStep(step);
    return sb;
}

QWidget* labeledAxis(QWidget* parent, const QString& label, QDoubleSpinBox* sb)
{
    auto* w = new QWidget(parent);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(2);
    auto* axisLabel = new QLabel(label, w);
    axisLabel->setAlignment(Qt::AlignHCenter);
    axisLabel->setStyleSheet("color: #6272A4; font-size: 10px;");
    l->addWidget(axisLabel);
    l->addWidget(sb);
    return w;
}

} // namespace

template <int N>
QWidget* VectorDrawer<N>::build(const PropertyContext& pc)
{
    static const char* kLabels[] = {"X", "Y", "Z", "W"};

    auto* container = new QWidget();
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto field = *pc.field;
    void* inst = pc.componentPtr;
    float* vec = static_cast<float*>(field.get(inst));

    for (int i = 0; i < N && i < 4; ++i) {
        auto* sb = makeSpinBox(container, static_cast<double>(vec[i]), -99999.0, 99999.0, 3, 0.1);
        int idx = i;
        QObject::connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [pc, field, idx](double v) mutable {
                float* fv = static_cast<float*>(field.get(pc.componentPtr));
                float oldFv = fv[idx];
                fv[idx] = static_cast<float>(v);
                auto cmd = std::make_unique<SetFieldValueCommand>(
                    pc.entity, pc.componentName, field.getFieldName(),
                    dodoe::Json(std::to_string(oldFv)), dodoe::Json(std::to_string(v)));
                pc.ctx->commands().execute(std::move(cmd));
            });
        layout->addWidget(labeledAxis(container, kLabels[i], sb));
    }

    return container;
}

template <int N>
void VectorDrawer<N>::updateValue(const PropertyContext& /*pc*/) {}

template class VectorDrawer<2>;
template class VectorDrawer<3>;
template class VectorDrawer<4>;

} // namespace cakery
