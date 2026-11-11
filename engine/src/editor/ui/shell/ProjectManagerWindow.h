// do@Redlive
#pragma once
#include "dopch.h"

#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QScrollArea>
#include <QButtonGroup>
#include <QGridLayout>
#include <QHash>
#include <functional>
#include "services/ProjectConfig.h"

class QVBoxLayout;
class QStackedWidget;

namespace cakery {

class ProjectManagerTitleBar final : public QWidget {
public:
    explicit ProjectManagerTitleBar(QWidget* window);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void toggleMaximize();

    QWidget* m_window = nullptr;
};

class ProjectCard final : public QFrame {
public:
    ProjectCard(const ProjectEntry& entry, qint64 sizeBytes, int hue, int variant,
                QWidget* parent = nullptr);

    void setEntry(const ProjectEntry& entry, qint64 sizeBytes);
    void setSelected(bool selected);

    std::function<void()> onOpen;
    std::function<void()> onReveal;
    std::function<void()> onSettings;
    std::function<void()> onRemove;
    std::function<void()> onSelect;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawThumbnail(QPainter& painter, const QRect& rect) const;

    ProjectEntry m_entry;
    int m_hue = 38;
    int m_variant = 0;
    bool m_selected = false;
    QWidget* m_previewSpacer = nullptr;
    QWidget* m_tools = nullptr;
    QLabel* m_dot = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_pathLabel = nullptr;
    QLabel* m_metaLabel = nullptr;
    QLabel* m_tagPill = nullptr;
};

class ProjectManagerWindow : public QDialog {
    Q_OBJECT
public:
    explicit ProjectManagerWindow(ProjectConfig& config, QWidget* parent = nullptr);
    ~ProjectManagerWindow() override;

signals:
    void onProjectOpened(const QString& projectPath);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onOpenProject();
    void onNewProject();
    void onImportProject();
    void onScanFolder();
    void onRefresh();
    void onShowInExplorer();
    void onSearchChanged(const QString& text);
    void onSortChanged(int index);

private:
    QWidget* createTitleBar();
    QWidget* createSidebar();
    QWidget* createMainArea();
    void applyFilterList();
    void rebuildGrid();
    void updateChrome();
    void selectProject(const QString& projectFile);
    void openProjectFile(const QString& projectFile);
    void removeProjectFile(const QString& projectFile);
    void openSettingsFor(const QString& projectFile);
    qint64 cachedDirSize(const ProjectEntry& proj);

    QButtonGroup* m_filterGroup = nullptr;
    QButtonGroup* m_tagGroup = nullptr;
    QButtonGroup* m_tabGroup = nullptr;
    QPushButton* m_filterRecentBtn = nullptr;
    QPushButton* m_filterAllBtn = nullptr;
    QPushButton* m_filterArchiveBtn = nullptr;
    QPushButton* m_tagGameBtn = nullptr;
    QPushButton* m_tagDemoBtn = nullptr;
    QPushButton* m_tabRecent = nullptr;
    QPushButton* m_tabAll = nullptr;
    QPushButton* m_tabArchive = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_sortCombo = nullptr;
    QLabel* m_viewTitle = nullptr;
    QLabel* m_viewSub = nullptr;
    QScrollArea* m_scroll = nullptr;
    QStackedWidget* m_stack = nullptr;
    QWidget* m_gridHost = nullptr;
    QGridLayout* m_grid = nullptr;
    QLabel* m_emptyHint = nullptr;
    QLabel* m_statCount = nullptr;
    QLabel* m_statSize = nullptr;
    QLabel* m_statSel = nullptr;
    QString m_filter = QStringLiteral("recent");
    QString m_tagFilter;
    QString m_selectedFile;
    QList<ProjectEntry> m_filtered;
    QList<ProjectCard*> m_cards;
    QHash<QString, qint64> m_sizeCache;
    ProjectConfig& m_config;
};

} // namespace cakery
