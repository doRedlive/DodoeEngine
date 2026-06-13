

#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QToolButton>
#include <QLabel>
#include <QSlider>
#include <QSplitter>

namespace cakery {

class ProjectBrowserWidget : public QWidget {
    Q_OBJECT
public:
    explicit ProjectBrowserWidget(QWidget* parent = nullptr);

    void setBasePath(const QString& path);
    void refresh();

private slots:
    void onFolderSelected();
    void onAssetDoubleClicked(QListWidgetItem* item);
    void onIconSizeChanged(int value);

private:
    void buildFolderTree(const QString& rootPath);
    void populateAssets(const QString& folderPath);


    QToolButton* m_createBtn = nullptr;
    QLineEdit* m_searchEdit = nullptr;


    QSplitter* m_splitter = nullptr;
    QTreeWidget* m_folderTree = nullptr;
    QLabel* m_breadcrumb = nullptr;
    QListWidget* m_assetList = nullptr;


    QLabel* m_pathLabel = nullptr;
    QSlider* m_iconSizeSlider = nullptr;


    QString m_basePath;
    QString m_currentFolder;
};

}
