// do@Redlive

#include "CompositeDrawer.h"
#include "property/PropertyDrawer.h"
#include "runtime/core/meta/reflection/reflection.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

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

    void* subInstance = pc.field->get(pc.componentPtr);
    if (!subInstance) {
        delete[] fields;
        return new QLabel(QString("(%1) null").arg(typeName));
    }

    auto* root = new QWidget();
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(1);

    auto* header = new QHBoxLayout();
    auto* foldBtn = new QToolButton();
    foldBtn->setArrowType(Qt::DownArrow);
    foldBtn->setAutoRaise(true);
    foldBtn->setCheckable(true);
    foldBtn->setChecked(true);

    auto* nameLabel = new QLabel(QString::fromUtf8(pc.field->getFieldName()));
    nameLabel->setStyleSheet("color:#C8C8C8; font-size:11px; font-weight:bold;");

    header->addWidget(foldBtn);
    header->addWidget(nameLabel);
    header->addStretch();
    rootLayout->addLayout(header);

    auto* content = new QWidget();
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(12, 0, 0, 0);
    contentLayout->setSpacing(1);

    QObject::connect(foldBtn, &QToolButton::toggled, [foldBtn, content](bool checked) {
        foldBtn->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        content->setVisible(checked);
    });

    auto& reg = PropertyDrawerRegistry::self();
    for (int i = 0; i < count; ++i) {
        if (fields[i].isHidden()) continue;

        const char* headerText = fields[i].attribute("Header");
        if (headerText && headerText[0]) {
            auto* headerLabel = new QLabel(QString::fromUtf8(headerText));
            headerLabel->setStyleSheet("color:#4EC9B0; font-size:11px; font-weight:bold;");
            contentLayout->addWidget(headerLabel);
        }

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
                contentLayout->addLayout(row);
            }
        }
    }

    delete[] fields;
    return root;
}

void CompositeDrawer::updateValue(const PropertyContext&) {}

} // namespace cakery
