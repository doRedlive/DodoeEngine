// do@Redlive
#pragma once
#include "dopch.h"

#include <QWidget>

#include "core/bridge/EditorTypes.h"


#include <functional>
#include <string>
#include <vector>

class QBoxLayout;
class QVBoxLayout;

namespace cakery {

class EditorRemoteWidget : public QWidget {
    Q_OBJECT
public:
    using Provider = std::function<bool(Json&)>;
    using Dispatcher =
        std::function<void(const String& controlId, const String& event, const Json& value)>;

    struct PropertyContext {
        Json value = nullptr;
        DynamicArray<InspectorFieldMetadata> fields;
        std::function<void(const Json&)> onChanged;
    };

    EditorRemoteWidget(Provider provider, Dispatcher dispatcher, QWidget* parent = nullptr);
    EditorRemoteWidget(Provider provider, Dispatcher dispatcher, PropertyContext property,
                       QWidget* parent = nullptr);

    void refresh();
    bool rebuildIfChanged();

private:
    void rebuildFrom(const Json& tree);
    void buildNodes(QBoxLayout* layout, const Json& nodes);
    void buildNode(QBoxLayout* layout, const Json& node);
    void buildPropertyNode(QBoxLayout* layout, const Json& node);
    void emitEvent(const String& controlId, const String& event, const Json& value);

    Provider m_provider;
    Dispatcher m_dispatcher;
    PropertyContext m_property;
    QVBoxLayout* m_root = nullptr;
    Json m_tree = nullptr;
    String m_lastJson;
    bool m_building = false;
};

} // namespace cakery
