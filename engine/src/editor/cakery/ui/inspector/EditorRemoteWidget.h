// do@Redlive

#pragma once

#include <QWidget>

#include "bridge/EditorBackend.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <string>
#include <vector>

class QBoxLayout;
class QVBoxLayout;

namespace cakery {

class EditorRemoteWidget : public QWidget {
    Q_OBJECT
public:
    using Provider = std::function<bool(nlohmann::json&)>;
    using Dispatcher =
        std::function<void(const std::string& controlId, const std::string& event, const nlohmann::json& value)>;

    struct PropertyContext {
        nlohmann::json value = nullptr;
        std::vector<InspectorFieldMetadata> fields;
        std::function<void(const nlohmann::json&)> onChanged;
    };

    EditorRemoteWidget(Provider provider, Dispatcher dispatcher, QWidget* parent = nullptr);
    EditorRemoteWidget(Provider provider, Dispatcher dispatcher, PropertyContext property,
                       QWidget* parent = nullptr);

    void refresh();
    bool rebuildIfChanged();

private:
    void rebuildFrom(const nlohmann::json& tree);
    void buildNodes(QBoxLayout* layout, const nlohmann::json& nodes);
    void buildNode(QBoxLayout* layout, const nlohmann::json& node);
    void buildPropertyNode(QBoxLayout* layout, const nlohmann::json& node);
    void emitEvent(const std::string& controlId, const std::string& event, const nlohmann::json& value);

    Provider m_provider;
    Dispatcher m_dispatcher;
    PropertyContext m_property;
    QVBoxLayout* m_root = nullptr;
    nlohmann::json m_tree = nullptr;
    std::string m_lastJson;
    bool m_building = false;
};

} // namespace cakery
