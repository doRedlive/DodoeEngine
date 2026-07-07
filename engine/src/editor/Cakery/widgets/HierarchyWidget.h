// do@Redlive

#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDropEvent>

namespace dodoe { class Entity; }

namespace cakery {

class HierarchyWidget : public QWidget {
    Q_OBJECT
public:
    explicit HierarchyWidget(QWidget* parent = nullptr);

    void refresh();

signals:
    void entitySelected(dodoe::Entity entity);
    void entityDeselected();

private slots:
    void onSearchChanged(const QString& text);
    void onCreateEntity();
    void onDeleteSelected();
    void onItemSelected();
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void populateItem(QTreeWidgetItem* parentItem, dodoe::Entity entity);
    bool matchesSearch(QTreeWidgetItem* item) const;
    void handleExternalDrop(QDropEvent* event);
    void createEntityFromAsset(const QString& filePath, dodoe::Entity parent);

    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_createBtn = nullptr;
    QTreeWidget* m_tree = nullptr;
    QString m_searchText;
};

} // namespace cakery
