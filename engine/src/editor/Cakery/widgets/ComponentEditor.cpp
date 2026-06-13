// do@Redlive

#include "ComponentEditor.h"
#include "runtime/function/world/entity.h"

namespace cakery {

ComponentEditor::ComponentEditor(const QString& typeName, dodoe::Entity entity,
                                 bool canRemove, QWidget* parent)
    : QWidget(parent)
    , m_typeName(typeName)
    , m_entity(entity)
    , m_canRemove(canRemove)
{
}

} // namespace cakery
