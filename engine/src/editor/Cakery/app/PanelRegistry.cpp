// do@Redlive

#include "PanelRegistry.h"

#include "Cakery/panels/ScenePanel.h"
#include "Cakery/panels/GamePanel.h"
#include "Cakery/panels/HierarchyPanel.h"
#include "Cakery/panels/InspectorPanel.h"
#include "Cakery/panels/ConsolePanel.h"
#include "Cakery/panels/ProjectPanel.h"
#include "Cakery/panels/TerminalPanel.h"
#include "Cakery/panels/TilePalettePanel.h"
#include "Cakery/panels/MemoryPanel.h"

namespace cakery {

PanelRegistry& PanelRegistry::self()
{
    static PanelRegistry instance;
    return instance;
}

void PanelRegistry::registerPanel(const std::string& factoryName, Factory f)
{
    m_factories[factoryName] = std::move(f);
}

Panel* PanelRegistry::create(const std::string& factoryName, EditorContext& ctx, QWidget* parent) const
{
    auto it = m_factories.find(factoryName);
    if (it != m_factories.end()) {
        return it->second(ctx, parent);
    }
    return nullptr;
}

void PanelRegistry::registerBuiltinPanels()
{
    registerPanel("Scene", [](EditorContext& ctx, QWidget* parent) -> Panel* {
        return new ScenePanel(ctx, parent);
    });
    registerPanel("Game", [](EditorContext& ctx, QWidget* parent) -> Panel* {
        return new GamePanel(ctx, parent);
    });
    registerPanel("Hierarchy", [](EditorContext& ctx, QWidget* parent) -> Panel* {
        return new HierarchyPanel(ctx, parent);
    });
    registerPanel("Inspector", [](EditorContext& ctx, QWidget* parent) -> Panel* {
        return new InspectorPanel(ctx, parent);
    });
    registerPanel("Project", [](EditorContext& ctx, QWidget* parent) -> Panel* {
        return new ProjectPanel(ctx, parent);
    });
    registerPanel("Console", [](EditorContext& ctx, QWidget* parent) -> Panel* {
        return new ConsolePanel(ctx, parent);
    });
    registerPanel("Terminal", [](EditorContext& ctx, QWidget* parent) -> Panel* {
        return new TerminalPanel(ctx, parent);
    });
    registerPanel("TilePalette", [](EditorContext& ctx, QWidget* parent) -> Panel* {
        return new TilePalettePanel(ctx, parent);
    });
    registerPanel("Memory", [](EditorContext& ctx, QWidget* parent) -> Panel* {
        return new MemoryPanel(ctx, parent);
    });
}

} // namespace cakery
