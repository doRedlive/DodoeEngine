// do@Redlive

#include "LayoutManager.h"

#include <DockManager.h>

namespace cakery {

void LayoutManager::applyDefault()
{
}

void LayoutManager::saveNamed(const QString& name)
{
    m_dm->addPerspective(name);
}

void LayoutManager::loadNamed(const QString& name)
{
    m_dm->openPerspective(name);
}

void LayoutManager::deleteNamed(const QString& name)
{
    m_dm->removePerspective(name);
}

QStringList LayoutManager::namedLayouts() const
{
    return m_dm->perspectiveNames();
}

void LayoutManager::restoreSession()
{
}

void LayoutManager::saveSession()
{
}

void LayoutManager::applyPreset(const QString& /*preset*/)
{
}

} // namespace cakery
