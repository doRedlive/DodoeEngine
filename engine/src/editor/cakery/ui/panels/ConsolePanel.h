// do@Redlive

#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include <vector>

class QComboBox;
class QLineEdit;
class QListWidget;
class QTimer;

namespace cakery {

class EditorWorkspaceContext;

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

    void append(ConsoleLogLevel level, const QString& message, const QString& source = {});

private:
    struct Entry {
        ConsoleLogLevel level;
        QString message;
        QString source;
    };

    void refresh();
    void syncBackendLogs();
    void copySelection();
    void clearEntries();
    static QString levelName(ConsoleLogLevel level);
    static QColor levelColor(ConsoleLogLevel level);

    QLineEdit* m_search = nullptr;
    QComboBox* m_levelFilter = nullptr;
    QListWidget* m_list = nullptr;
    std::vector<Entry> m_entries;
    std::vector<Entry> m_backendEntries;
    EditorWorkspaceContext& m_context;
    QTimer* m_refreshTimer = nullptr;
};

} // namespace cakery
