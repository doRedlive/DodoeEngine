// do@Redlive

#include "ReadOnlyDrawer.h"

#include "runtime/core/meta/reflection/reflection.h"

#include <QLabel>
#include <QHBoxLayout>

namespace cakery {

QWidget* ReadOnlyDrawer::build(const PropertyContext& pc)
{
    auto* container = new QWidget();
    auto* layout    = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* nameLabel = new QLabel(QString::fromUtf8(pc.field->getFieldName()));
    nameLabel->setStyleSheet("color:#c8c8c8;");
    layout->addWidget(nameLabel);

    auto* typeLabel = new QLabel(QString("(%1)").arg(QString::fromUtf8(pc.field->getFieldTypeName())));
    typeLabel->setStyleSheet("color:#808080; font-style:italic;");
    layout->addWidget(typeLabel);

    layout->addStretch();
    return container;
}

void ReadOnlyDrawer::updateValue(const PropertyContext& /*pc*/)
{
}

} // namespace cakery
