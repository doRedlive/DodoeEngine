// do@Redlive

#include "TerminalPanel.h"
#include "framework/EditorContext.h"
#include "framework/console/CommandRegistry.h"

#include <QVBoxLayout>
#include <QCompleter>
#include <QStringListModel>

namespace cakery {

TerminalPanel::TerminalPanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);

    m_output = new QListWidget(this);
    layout->addWidget(m_output, 1);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText("> entity.create name=Foo");
    layout->addWidget(m_input);

    QStringList names;
    for (auto& spec : CommandRegistry::self().list()) {
        names << QString::fromStdString(spec.name);
    }
    auto* completer = new QCompleter(names, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_input->setCompleter(completer);

    connect(m_input, &QLineEdit::returnPressed, [this]() {
        QString text = m_input->text().trimmed();
        if (!text.isEmpty()) {
            m_history.append(text);
            m_historyPos = m_history.size();
            submit(text);
            m_input->clear();
        }
    });
}

void TerminalPanel::submit(const QString& line)
{
    auto* userItem = new QListWidgetItem("> " + line);
    userItem->setForeground(QColor(100, 149, 237));
    m_output->addItem(userItem);

    auto result = CommandRegistry::self().execute(m_ctx, line.toStdString());

    auto* resultItem = new QListWidgetItem();
    if (result.ok) {
        resultItem->setText(QString::fromStdString(result.message));
        resultItem->setForeground(QColor(180, 180, 180));
    } else {
        resultItem->setText(QString::fromStdString(result.message));
        resultItem->setForeground(QColor(255, 100, 100));
    }
    m_output->addItem(resultItem);
    m_output->scrollToBottom();
}

} // namespace cakery
