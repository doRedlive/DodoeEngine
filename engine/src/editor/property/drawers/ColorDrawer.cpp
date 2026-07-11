// do@Redlive

#include "ColorDrawer.h"
#include "framework/EditorContext.h"
#include "framework/command/commands/SetFieldValueCommand.h"
#include "framework/command/CommandStack.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/util.h"
#include "runtime/core/utils/json.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QColorDialog>
#include <QSignalBlocker>
#include <cmath>

namespace cakery {

QWidget* ColorDrawer::build(const PropertyContext& pc)
{
    auto* container = new QWidget();
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* field = pc.field;
    dodoe::Color* color = static_cast<dodoe::Color*>(field->get(pc.componentPtr));

    auto* btn = new QPushButton(container);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(28, 28);

    auto* r = new QDoubleSpinBox(); r->setRange(0.0, 1.0); r->setDecimals(3); r->setSingleStep(0.05); r->setValue(static_cast<double>(color->r));
    auto* g = new QDoubleSpinBox(); g->setRange(0.0, 1.0); g->setDecimals(3); g->setSingleStep(0.05); g->setValue(static_cast<double>(color->g));
    auto* b = new QDoubleSpinBox(); b->setRange(0.0, 1.0); b->setDecimals(3); b->setSingleStep(0.05); b->setValue(static_cast<double>(color->b));
    auto* a = new QDoubleSpinBox(); a->setRange(0.0, 1.0); a->setDecimals(3); a->setSingleStep(0.05); a->setValue(static_cast<double>(color->a));

    auto updateBtn = [btn](float cr, float cg, float cb, float ca) {
        int ir = static_cast<int>(std::round(cr * 255.0f));
        int ig = static_cast<int>(std::round(cg * 255.0f));
        int ib = static_cast<int>(std::round(cb * 255.0f));
        int ia = static_cast<int>(std::round(ca * 255.0f));
        btn->setStyleSheet(QString(
            "QPushButton { border: 1px solid #44475A; border-radius: 3px; "
            "background-color: rgba(%1, %2, %3, %4); }"
            "QPushButton:hover { border-color: #6272A4; }"
        ).arg(ir).arg(ig).arg(ib).arg(ia));
    };

    updateBtn(color->r, color->g, color->b, color->a);

    auto apply = [pc, field](float cr, float cg, float cb, float ca) {
        dodoe::Color* col = static_cast<dodoe::Color*>(field->get(pc.componentPtr));
        dodoe::Color old = *col;
        col->r = cr; col->g = cg; col->b = cb; col->a = ca;
        auto cmd = std::make_unique<SetFieldValueCommand>(
            pc.entity, pc.componentName, field->getFieldName(),
            dodoe::Json(""), dodoe::Json(""));
        pc.ctx->commands().execute(std::move(cmd));
    };

    auto onSpinChanged = [r, g, b, a, &updateBtn, &apply](double) {
        float cr = static_cast<float>(r->value());
        float cg = static_cast<float>(g->value());
        float cb = static_cast<float>(b->value());
        float ca = static_cast<float>(a->value());
        updateBtn(cr, cg, cb, ca);
        apply(cr, cg, cb, ca);
    };

    QObject::connect(r, QOverload<double>::of(&QDoubleSpinBox::valueChanged), onSpinChanged);
    QObject::connect(g, QOverload<double>::of(&QDoubleSpinBox::valueChanged), onSpinChanged);
    QObject::connect(b, QOverload<double>::of(&QDoubleSpinBox::valueChanged), onSpinChanged);
    QObject::connect(a, QOverload<double>::of(&QDoubleSpinBox::valueChanged), onSpinChanged);

    QObject::connect(btn, &QPushButton::clicked, [container, r, g, b, a, updateBtn, apply]() {
        QColor cur = QColor::fromRgbF(r->value(), g->value(), b->value(), a->value());
        QColor chosen = QColorDialog::getColor(cur, container, "Choose Color", QColorDialog::ShowAlphaChannel);
        if (!chosen.isValid()) return;
        QSignalBlocker br(r), bg(g), bb(b), ba(a);
        r->setValue(chosen.redF());
        g->setValue(chosen.greenF());
        b->setValue(chosen.blueF());
        a->setValue(chosen.alphaF());
        float cr = static_cast<float>(r->value());
        float cg = static_cast<float>(g->value());
        float cb = static_cast<float>(b->value());
        float ca = static_cast<float>(a->value());
        updateBtn(cr, cg, cb, ca);
        apply(cr, cg, cb, ca);
    });

    layout->addWidget(btn);

    auto lbl = [](QWidget* p, const QString& t, QDoubleSpinBox* sb) {
        auto* w = new QWidget(p);
        auto* l = new QVBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(2);
        auto* al = new QLabel(t, w);
        al->setAlignment(Qt::AlignHCenter);
        al->setStyleSheet("color: #6272A4; font-size: 10px;");
        l->addWidget(al);
        l->addWidget(sb);
        return w;
    };

    layout->addWidget(lbl(container, "R", r));
    layout->addWidget(lbl(container, "G", g));
    layout->addWidget(lbl(container, "B", b));
    layout->addWidget(lbl(container, "A", a));

    return container;
}

void ColorDrawer::updateValue(const PropertyContext& /*pc*/) {}

} // namespace cakery
