// do@Redlive
#pragma once
#include "dopch.h"

#include <QPointer>
#include <QWidget>

#include "core/bridge/EditorTypes.h"
#include "core/Signal.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>


class QLineEdit;
class QScrollArea;
class QVBoxLayout;

namespace cakery {

class EditorWorkspaceContext;
class InspectorViewModel;

class InspectorPanel : public QWidget {
    Q_OBJECT
public:
    explicit InspectorPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);
    ~InspectorPanel() override;
    void inspectPath(const QString& path);

private:
    void refresh();
    void onViewModelChanged();
    void onRenameEntity(const QString& name);
    void commitComponentValue(Size_t index, bool managed, const Json& value,
                              const QString& fieldPath = QString());

    Scope<InspectorViewModel> m_viewModel;
    QVBoxLayout* m_layout = nullptr;
    QScrollArea* m_scroll = nullptr;
    QPointer<QLineEdit> m_filter = nullptr;
    QString m_filterText;
    QString m_browserPath;
    bool m_filterFocused = false;
    int m_scrollPosition = 0;
    QPointer<QLineEdit> m_nameEdit = nullptr;
    DynamicArray<bool> m_componentExpanded;
    DynamicArray<bool> m_managedComponentExpanded;
    bool m_editing = false;
    String m_componentClipboardType;
    Json m_componentClipboardValue;
    ScopedConnection m_viewModelSubscription;
};

} // namespace cakery
