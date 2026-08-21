// do@Redlive

#pragma once

#include "cakery/panels/Panel.h"

#include <QWidget>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace cakery {

class PanelRegistry {
public:
    static PanelRegistry& self();
    using Factory = std::function<Panel*(EditorContext&, QWidget*)>;

    void registerPanel(const std::string& factoryName, Factory f);
    Panel* create(const std::string& factoryName, EditorContext& ctx, QWidget* parent) const;
    void registerBuiltinPanels();

private:
    PanelRegistry() = default;

    std::unordered_map<std::string, Factory> m_factories;
};

} // namespace cakery
