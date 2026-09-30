// do@Redlive

#pragma once

#include <QWidget>

#include <utility>
#include <vector>

#include "cakery/ui/script/CSharpSymbolIndex.h"

class QCheckBox;
class QCompleter;
class QFileSystemWatcher;
class QLabel;
class QLineEdit;
class QStringListModel;
class QStackedWidget;
class QTabWidget;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace cakery {

class CSharpCodeEditor;
class CSharpHighlighter;
class EditorWorkspaceContext;

class ScriptPanel : public QWidget {
    Q_OBJECT
public:
    explicit ScriptPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);

    void openFile(const QString& path);
    void reopenProject();
    bool hasUnsavedChanges() const;
    bool saveAll();
    void discardAll();
    void closeAll(bool force);

signals:
    void dirtyChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct EditorPage {
        QString path;
        CSharpCodeEditor* editor = nullptr;
        CSharpHighlighter* highlighter = nullptr;
    };

    int pageIndex(const QString& path) const;
    EditorPage* currentPage();
    const EditorPage* currentPage() const;

    void savePage(int index);
    void saveCurrent();
    void reloadPage(int index);
    void closeTab(int index);
    void closeCurrentTab();
    void updateTabText(int index);
    void setCurrentTab(int index);

    void ensureIndexBuilt();
    void scheduleIndexRebuild();

    void rebuildOutline();
    void applyOutlineFilter();
    void jumpToOutlineItem(QTreeWidgetItem* item, int column);

    void jumpTo(const QString& path, int line, int column, int selectLength,
                bool recordHistory);
    void navigateToSymbol(const QString& symbol);
    void navigateBack();
    void pushHistory();

    void updateCompletion();
    void insertCompletion(const QString& completion);
    void rebuildCompletionModel();

    void showFindBar(bool withReplace);
    void hideFindBar();
    void search(bool backward);
    void findNext();
    void findPrevious();
    void replaceCurrent();
    void replaceAllMatches();
    int countMatches() const;

    void updateStatus();
    void onWatcherFileChanged(const QString& path);
    void updateEmptyState();

    EditorWorkspaceContext& m_context;
    CSharpSymbolIndex m_index;
    QStackedWidget* m_stack = nullptr;
    QTabWidget* m_tabs = nullptr;
    QTreeWidget* m_outline = nullptr;
    QLineEdit* m_outlineFilter = nullptr;
    QWidget* m_findBar = nullptr;
    QLineEdit* m_findEdit = nullptr;
    QLineEdit* m_replaceEdit = nullptr;
    QCheckBox* m_caseSensitive = nullptr;
    QToolButton* m_replaceButton = nullptr;
    QToolButton* m_replaceAllButton = nullptr;
    QLabel* m_matchLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QCompleter* m_completer = nullptr;
    QStringListModel* m_completionModel = nullptr;
    QFileSystemWatcher* m_watcher = nullptr;
    QTimer* m_outlineTimer = nullptr;
    QTimer* m_indexTimer = nullptr;
    QVector<EditorPage> m_pages;
    QVector<std::pair<QString, int>> m_history;
    int m_historyPosition = -1;
    int m_suppressWatcher = 0;
};

} // namespace cakery
