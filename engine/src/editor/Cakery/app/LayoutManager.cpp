// do@Redlive

#include "LayoutManager.h"
#include "PanelRegistry.h"
#include "framework/EditorContext.h"
#include "framework/config/EditorConfig.h"

#include "runtime/core/utils/json.h"
#include "runtime/function/log/log_system.h"

#include <DockManager.h>
#include <DockWidget.h>
#include <DockAreaWidget.h>

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace cakery {

LayoutManager::LayoutManager(ads::CDockManager* dm, EditorContext& ctx)
    : m_dm(dm), m_ctx(ctx)
{
}

LayoutManager::~LayoutManager() = default;

ads::CDockWidget* LayoutManager::createDock(const std::string& id, const std::string& factory)
{
    auto* panel = PanelRegistry::self().create(factory, m_ctx, nullptr);
    if (!panel) {
        LOG_WARN("[LayoutManager] Unknown panel factory: {}", factory);
        return nullptr;
    }

    auto* dock = new ads::CDockWidget(QString::fromStdString(id));
    dock->setObjectName(QString::fromStdString(id));
    dock->setWidget(panel);
    return dock;
}

void LayoutManager::applyPreset(const std::string& name)
{
    dodoe::Json layout = EditorConfig::self().layoutJson(name);
    if (!layout.contains("panels") || !layout["panels"].is_array()) {
        LOG_ERROR("[LayoutManager] Invalid layout: {}", name);
        applyDefault();
        return;
    }

    ads::CDockWidget* centralDock = nullptr;
    ads::DockWidgetArea centralArea = ads::CenterDockWidgetArea;
    std::unordered_map<std::string, ads::CDockWidget*> dockMap;

    for (auto& p : layout["panels"]) {
        std::string pid     = p.value("id", "");
        std::string factory = p.value("factory", "");
        std::string area    = p.value("area", "Center");
        bool central        = p.value("central", false);
        std::string tabWith = p.value("tabWith", "");
        std::string relativeTo = p.value("relativeTo", "");

        if (pid.empty() || factory.empty()) continue;

        auto* dock = createDock(pid, factory);
        if (!dock) continue;

        dockMap[pid] = dock;

        if (central) {
            centralDock = dock;
            m_dm->setCentralWidget(dock);
            continue;
        }

        if (!tabWith.empty() && dockMap.count(tabWith)) {
            m_dm->addDockWidget(ads::CenterDockWidgetArea, dock,
                                dockMap[tabWith]->dockAreaWidget());
            continue;
        }

        ads::DockWidgetArea adsArea;
        if (area == "Left")   adsArea = ads::LeftDockWidgetArea;
        else if (area == "Right")  adsArea = ads::RightDockWidgetArea;
        else if (area == "Top")    adsArea = ads::TopDockWidgetArea;
        else if (area == "Bottom") adsArea = ads::BottomDockWidgetArea;
        else adsArea = ads::CenterDockWidgetArea;

        ads::CDockAreaWidget* anchor = nullptr;
        if (!relativeTo.empty() && dockMap.count(relativeTo)) {
            anchor = dockMap[relativeTo]->dockAreaWidget();
        }
        if (!anchor && centralDock) {
            anchor = centralDock->dockAreaWidget();
        }

        m_dm->addDockWidget(adsArea, dock, anchor);
    }

    LOG_INFO("[LayoutManager] Applied preset: {}", name);
}

void LayoutManager::applyDefault()
{
    applyPreset(EditorConfig::self().defaultLayoutName());
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
    applyDefault();
}

void LayoutManager::saveSession()
{
}

} // namespace cakery
