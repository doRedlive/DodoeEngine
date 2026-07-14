// do@Redlive

#include "CompositeDrawer.h"
#include "property/PropertyDrawer.h"
#include "runtime/core/meta/reflection/reflection.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

namespace cakery {

QWidget* CompositeDrawer::build(const PropertyContext& pc)
{
    const char* typeName = pc.field->getFieldTypeName();
    if (!typeName) {
        return new QLabel("?");
    }

    dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName(typeName);
    if (!meta.isValid()) {
        return new QLabel(QString("(%1)").arg(typeName));
    }

    dodoe::FieldAccessor* fields = nullptr;
    int count = meta.get_field_list(fields);
    if (count <= 0) {
        return new QLabel(QString("(%1)").arg(typeName));
    }

    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 2, 0, 2);
    layout->setSpacing(1);

    void* subInstance = pc.field->get(pc.componentPtr);
    if (!subInstance) {
        delete[] fields;
        return new QLabel(QString("(%1) null").arg(typeName));
    }

    auto& reg = PropertyDrawerRegistry::self();
    for (int i = 0; i < count; ++i) {
        if (fields[i].isHidden()) continue;

        PropertyContext subPc;
        subPc.ctx           = pc.ctx;
        subPc.entity        = pc.entity;
        subPc.componentName = pc.componentName;
        subPc.componentPtr  = subInstance;
        subPc.field         = &fields[i];

        auto drawer = reg.create(fields[i]);
        if (drawer) {
            QWidget* w = drawer->build(subPc);
            if (w) {
                const char* tooltip = fields[i].attribute("Tooltip");
                if (tooltip && tooltip[0]) {
                    w->setToolTip(QString::fromUtf8(tooltip));
                }

                auto* row = new QHBoxLayout();
                auto* label = new QLabel(QString::fromUtf8(fields[i].getFieldName()));
                label->setMinimumWidth(80);
                label->setStyleSheet("color: #8C8C8C; font-size: 11px;");
                row->addWidget(label);
                row->addWidget(w, 1);
                layout->addLayout(row);
            }
        }
    }

    delete[] fields;
    return container;
}

void CompositeDrawer::updateValue(const PropertyContext&) {}

} // namespace cakery
