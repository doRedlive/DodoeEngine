// do@Redlive

#include "dopch.h"

#include "ProjectSelectionController.h"

#include "ProjectManagerWindow.h"

namespace cakery {

ProjectSelectionController::ProjectSelectionController(ProjectConfig& config, QObject* parent)
    : QObject(parent), m_config(config)
{
}

ProjectSelectionController::~ProjectSelectionController()
{
    if (m_window) {
        m_window->close();
        delete m_window;
        m_window = nullptr;
    }
}

void ProjectSelectionController::show()
{
    ensureWindow();
    m_window->show();
    m_window->raise();
    m_window->activateWindow();
}

void ProjectSelectionController::dismiss()
{
    if (!m_window) {
        return;
    }
    m_window->close();
    m_window->deleteLater();
    m_window = nullptr;
}

void ProjectSelectionController::ensureWindow()
{
    if (m_window) {
        return;
    }
    m_window = new ProjectManagerWindow(m_config);
    connect(m_window, &ProjectManagerWindow::onProjectOpened,
            this, &ProjectSelectionController::projectSelected);
}

} // namespace cakery
