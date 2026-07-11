// do@Redlive

#include "CompositeDrawer.h"
#include "runtime/core/meta/reflection/reflection.h"

#include <QLabel>

namespace cakery {

QWidget* CompositeDrawer::build(const PropertyContext& pc)
{
    const char* typeName = pc.field->getFieldTypeName();
    auto* label = new QLabel(QString("(%1)").arg(typeName ? typeName : "?"));
    label->setStyleSheet("color: #888; font-style: italic;");
    return label;
}

void CompositeDrawer::updateValue(const PropertyContext& /*pc*/) {}

} // namespace cakery
