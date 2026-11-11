// do@Redlive
#pragma once
#include "dopch.h"

#include <QColor>
#include <QString>
#include <QWidget>

#include "core/Signal.h"

#include <vector>

class QComboBox;
class QLineEdit;
class QListWidget;
class QTimer;

namespace cakery {

class EditorWorkspaceContext;
class ConsoleViewModel;

enum class ConsoleLogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
};

class ConsolePanel : public QWidget {
    Q_OBJECT
public:
    explicit ConsolePanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);
    ~ConsolePanel() override;

    void append(ConsoleLogLevel level, const QString& message, const QString& source = {});

private:
    void onViewModelChanged();
    void refresh();
    void syncBackendLogs();
    void copySelection();
    void clearEntries();
    static QString levelName(ConsoleLogLevel level);
    static QColor levelColor(ConsoleLogLevel level);

    QLineEdit* m_search = nullptr;
    QComboBox* m_levelFilter = nullptr;
    QListWidget* m_list = nullptr;
    Scope<ConsoleViewModel> m_viewModel;
    ScopedConnection m_viewModelConnection;
    QTimer* m_refreshTimer = nullptr;
};

} // namespace cakery
