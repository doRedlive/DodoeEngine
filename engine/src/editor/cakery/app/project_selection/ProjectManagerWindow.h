// do@Redlive

#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include "ProjectConfig.h"

namespace cakery {

class ProjectManagerWindow : public QDialog {
    Q_OBJECT
public:
    explicit ProjectManagerWindow(QWidget* parent = nullptr);

signals:
    void onProjectOpened(const QString& projectPath);

private slots:
    void onOpenProject();
    void onNewProject();
    void onImportProject();
    void onScanFolder();
    void onRefresh();
    void onProjectSelected();
    void onSearchChanged(const QString& text);
    void onSortChanged(int index);

private:
    void rebuildList();
    void updateDetailPanel();

    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_sortCombo = nullptr;
    QListWidget* m_projectList = nullptr;
    QWidget* m_detailPanel = nullptr;
    QLabel* m_detailName = nullptr;
    QLabel* m_detailPath = nullptr;
    QLabel* m_detailDate = nullptr;
    QLabel* m_detailDesc = nullptr;
    QPushButton* m_openBtn = nullptr;

    QList<ProjectEntry> m_filtered;
    int m_selectedIndex = -1;
};

} // namespace cakery
