// do@Redlive

#include "dopch.h"

#include "EditorLayoutController.h"

#include <DockManager.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace cakery {

EditorLayoutController::EditorLayoutController(ads::CDockManager* dockManager,
                                               const QString& fileName)
    : m_dockManager(dockManager)
{
    m_statePath = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)).filePath(fileName);
}

void EditorLayoutController::captureDefault()
{
    if (m_dockManager) {
        m_defaultState = m_dockManager->saveState(1);
    }
}

void EditorLayoutController::restore()
{
    if (!m_dockManager || m_statePath.isEmpty()) {
        return;
    }
    QFile file(m_statePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QByteArray state = file.readAll();
    if (!state.isEmpty() && !m_dockManager->restoreState(state, 1)) {
        reset();
    }
}

void EditorLayoutController::save() const
{
    if (!m_dockManager || m_statePath.isEmpty()) {
        return;
    }
    QDir().mkpath(QFileInfo(m_statePath).absolutePath());
    QFile file(m_statePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(m_dockManager->saveState(1));
    }
}

void EditorLayoutController::reset()
{
    if (m_dockManager && !m_defaultState.isEmpty()) {
        m_dockManager->restoreState(m_defaultState, 1);
    }
}

} // namespace cakery
