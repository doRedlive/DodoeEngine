// do@Redlive

#include "ArrayDrawer.h"

#include "runtime/core/meta/reflection/reflection.h"

#include <QLabel>
#include <QVBoxLayout>

namespace cakery {

namespace {

QString formatElement(const char* typeName, void* elem)
{
    if (!typeName || !elem) {
        return QString("(null)");
    }

    std::string t = typeName;

    if (t == "float")  return QString::number(*static_cast<float*>(elem), 'g', 6);
    if (t == "double") return QString::number(*static_cast<double*>(elem), 'g', 8);
    if (t == "int" || t == "int32_t")  return QString::number(*static_cast<int*>(elem));
    if (t == "uint32_t") return QString::number(*static_cast<uint32_t*>(elem));
    if (t == "bool")  return (*static_cast<bool*>(elem)) ? QString("true") : QString("false");
    if (t == "String" || t == "std::string") return QString::fromUtf8(static_cast<dodoe::String*>(elem)->c_str());

    if (t == "Vector2f") { auto* v = static_cast<dodoe::Vector2f*>(elem); return QString("(%1, %2)").arg(v->x).arg(v->y); }
    if (t == "Vector3f") { auto* v = static_cast<dodoe::Vector3f*>(elem); return QString("(%1, %2, %3)").arg(v->x).arg(v->y).arg(v->z); }
    if (t == "Vector4f") { auto* v = static_cast<dodoe::Vector4f*>(elem); return QString("(%1, %2, %3, %4)").arg(v->x).arg(v->y).arg(v->z).arg(v->w); }
    if (t == "Vector2i") { auto* v = static_cast<dodoe::Vector2i*>(elem); return QString("(%1, %2)").arg(v->x).arg(v->y); }
    if (t == "Vector3i") { auto* v = static_cast<dodoe::Vector3i*>(elem); return QString("(%1, %2, %3)").arg(v->x).arg(v->y).arg(v->z); }
    if (t == "Vector4i") { auto* v = static_cast<dodoe::Vector4i*>(elem); return QString("(%1, %2, %3, %4)").arg(v->x).arg(v->y).arg(v->z).arg(v->w); }
    if (t == "Color") { auto* c = static_cast<dodoe::Color*>(elem); return QString("(%1, %2, %3, %4)").arg(c->r).arg(c->g).arg(c->b).arg(c->a); }

    return QString("(%1)").arg(QString::fromUtf8(typeName));
}

} // namespace

QWidget* ArrayDrawer::build(const PropertyContext& pc)
{
    auto* container = new QWidget();
    auto* layout    = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto* field = *pc.field;

    dodoe::ArrayAccessor accessor;
    if (!dodoe::TypeMeta::new_array_accessor_from_name(field.getFieldTypeName(), accessor)) {
        auto* fallback = new QLabel(QString("(%1)").arg(QString::fromUtf8(field.getFieldTypeName())));
        fallback->setStyleSheet("color:#808080; font-style:italic;");
        layout->addWidget(fallback);
        return container;
    }

    const int size = accessor.get_size(pc.componentPtr);

    auto* header = new QLabel(QString("%1 [%2]").arg(QString::fromUtf8(field.getFieldName())).arg(size));
    header->setStyleSheet("color:#c8c8c8;");
    layout->addWidget(header);

    const char* elementTypeName = accessor.get_element_type_name();
    for (int i = 0; i < size; ++i) {
        void* elem = accessor.get(i, pc.componentPtr);
        auto* row  = new QLabel(QString("  [%1]  %2").arg(i).arg(formatElement(elementTypeName, elem)));
        row->setStyleSheet("color:#9a9a9a;");
        layout->addWidget(row);
    }

    return container;
}

void ArrayDrawer::updateValue(const PropertyContext& /*pc*/)
{
}

} // namespace cakery
