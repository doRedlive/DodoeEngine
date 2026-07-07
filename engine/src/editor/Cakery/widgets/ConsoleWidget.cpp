#include "ConsoleWidget.h"
#include "services/LogService.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QMenu>

namespace cakery {

ConsoleWidget::ConsoleWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);
    layout->setSpacing(3);

    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(4);

    m_btnClear = new QToolButton(this);
    m_btnClear->setText(QString::fromUtf8("Clear"));
    m_btnClear->setPopupMode(QToolButton::InstantPopup);
    auto* clearMenu = new QMenu(this);
    clearMenu->addAction(tr("Clear"), this, &ConsoleWidget::onClear);
    clearMenu->addAction(tr("Clear on Play"));
    m_btnClear->setMenu(clearMenu);
    toolbar->addWidget(m_btnClear);

    m_btnCollapse = new QToolButton(this);
    m_btnCollapse->setText(tr("Collapse"));
    m_btnCollapse->setCheckable(true);
    m_btnCollapse->setChecked(true);
    toolbar->addWidget(m_btnCollapse);

    m_btnClearOnPlay = new QToolButton(this);
    m_btnClearOnPlay->setText(tr("Clear on Play"));
    m_btnClearOnPlay->setCheckable(true);
    toolbar->addWidget(m_btnClearOnPlay);

    m_btnErrorPause = new QToolButton(this);
    m_btnErrorPause->setText(tr("Error Pause"));
    m_btnErrorPause->setCheckable(true);
    toolbar->addWidget(m_btnErrorPause);

    toolbar->addStretch();

    m_btnFilterLog = new QToolButton(this);
    m_btnFilterLog->setText(tr("Log 0"));
    m_btnFilterLog->setCheckable(true);
    m_btnFilterLog->setChecked(true);
    toolbar->addWidget(m_btnFilterLog);

    m_btnFilterWarn = new QToolButton(this);
    m_btnFilterWarn->setText(tr("Warnings 0"));
    m_btnFilterWarn->setStyleSheet("color: #F1FA8C;");
    m_btnFilterWarn->setCheckable(true);
    m_btnFilterWarn->setChecked(true);
    toolbar->addWidget(m_btnFilterWarn);

    m_btnFilterError = new QToolButton(this);
    m_btnFilterError->setText(tr("Errors 0"));
    m_btnFilterError->setStyleSheet("color: #FF5555;");
    m_btnFilterError->setCheckable(true);
    m_btnFilterError->setChecked(true);
    toolbar->addWidget(m_btnFilterError);

    layout->addLayout(toolbar);

    m_list = new QListWidget(this);
    m_list->setFont(QFont("Consolas", 11));
    m_list->setObjectName("consoleList");
    layout->addWidget(m_list, 1);

    connect(m_btnFilterLog, &QToolButton::toggled, this, &ConsoleWidget::onFilterToggled);
    connect(m_btnFilterWarn, &QToolButton::toggled, this, &ConsoleWidget::onFilterToggled);
    connect(m_btnFilterError, &QToolButton::toggled, this, &ConsoleWidget::onFilterToggled);

    auto& logService = LogService::getInstance();
    connect(&logService, &LogService::updated, this, &ConsoleWidget::onLogUpdated);
}

void ConsoleWidget::onLogUpdated()
{
    refreshDisplay();
}

void ConsoleWidget::onClear()
{
    LogService::getInstance().clear();
}

void ConsoleWidget::onFilterToggled()
{
    m_showLog = m_btnFilterLog->isChecked();
    m_showWarn = m_btnFilterWarn->isChecked();
    m_showError = m_btnFilterError->isChecked();
    refreshDisplay();
}

void ConsoleWidget::onSearchChanged(const QString& text)
{
    for (int i = 0; i < m_list->count(); ++i) {
        auto* item = m_list->item(i);
        item->setHidden(!text.isEmpty() && !item->text().contains(text, Qt::CaseInsensitive));
    }
}

void ConsoleWidget::refreshDisplay()
{
    m_list->clear();

    for (const auto& entry : LogService::getInstance().entries()) {
        int level = static_cast<int>(entry.level);
        if (level <= 2 && !m_showLog) continue;
        if (level == 3 && !m_showWarn) continue;
        if (level >= 4 && !m_showError) continue;

        auto* item = new QListWidgetItem();
        QString text = QString("[%1] [%2] %3")
            .arg(entry.timestamp.toString("HH:mm:ss"))
            .arg(prefixForLevel(level))
            .arg(entry.message);

        if (entry.repeatCount > 1)
            text += QString(" (x%1)").arg(entry.repeatCount);

        item->setText(text);
        item->setForeground(colorForLevel(level));
        m_list->addItem(item);
    }

    m_list->scrollToBottom();
    updateFilterCounts();
}

void ConsoleWidget::updateFilterCounts()
{
    int logCount = 0, warnCount = 0, errCount = 0;

    for (const auto& entry : LogService::getInstance().entries()) {
        int level = static_cast<int>(entry.level);
        if (level <= 2) logCount++;
        else if (level == 3) warnCount++;
        else errCount++;
    }

    m_btnFilterLog->setText(QString("Log %1").arg(logCount));
    m_btnFilterWarn->setText(QString("Warnings %1").arg(warnCount));
    m_btnFilterError->setText(QString("Errors %1").arg(errCount));
}

QColor ConsoleWidget::colorForLevel(int level) const
{
    switch (level) {
    case 0: return QColor(150, 150, 150);
    case 1: return QColor(180, 180, 180);
    case 2: return QColor(200, 200, 200);
    case 3: return QColor(241, 250, 140);
    case 4: return QColor(255, 85, 85);
    case 5: return QColor(255, 50, 50);
    default: return QColor(200, 200, 200);
    }
}

QString ConsoleWidget::prefixForLevel(int level) const
{
    switch (level) {
    case 0: return "TRC";
    case 1: return "DBG";
    case 2: return "INF";
    case 3: return "WRN";
    case 4: return "ERR";
    case 5: return "CRT";
    default: return "???";
    }
}

}
