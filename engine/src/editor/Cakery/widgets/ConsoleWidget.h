#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QToolButton>
#include <QPushButton>
#include <QLabel>
#include "services/LogService.h"

namespace cakery {

class ConsoleWidget : public QWidget {
    Q_OBJECT
public:
    explicit ConsoleWidget(QWidget* parent = nullptr);

private slots:
    void onLogUpdated();
    void onClear();
    void onFilterToggled();
    void onSearchChanged(const QString& text);
    void refreshDisplay();

private:
    QColor colorForLevel(int level) const;
    QString prefixForLevel(int level) const;
    void updateFilterCounts();

    QListWidget* m_list = nullptr;
    QLineEdit* m_searchEdit = nullptr;

    QToolButton* m_btnClear = nullptr;
    QToolButton* m_btnCollapse = nullptr;
    QToolButton* m_btnClearOnPlay = nullptr;
    QToolButton* m_btnErrorPause = nullptr;

    QToolButton* m_btnFilterLog = nullptr;
    QToolButton* m_btnFilterWarn = nullptr;
    QToolButton* m_btnFilterError = nullptr;

    bool m_showLog = true;
    bool m_showWarn = true;
    bool m_showError = true;
};

}
