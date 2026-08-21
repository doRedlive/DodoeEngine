// do@Redlive

#pragma once

#include "Panel.h"
#include <QWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSplitter>

namespace cakery {

class ProjectPanel : public Panel {
    Q_OBJECT
public:
    explicit ProjectPanel(EditorContext& ctx, QWidget* parent = nullptr);

    void setBasePath(const QString& path);
    void refresh();

private:
    void populateFromAssetDatabase();

    QLineEdit* m_searchBox = nullptr;
    QComboBox* m_filterCombo = nullptr;
    QSplitter* m_splitter = nullptr;
    QTreeWidget* m_dirTree = nullptr;
    QListWidget* m_assetGrid = nullptr;
};

} // namespace cakery
