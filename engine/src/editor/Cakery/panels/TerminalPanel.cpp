// do@Redlive

#include "TerminalPanel.h"
#include "framework/EditorContext.h"

#include <QVBoxLayout>

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

    connect(m_input, &QLineEdit::returnPressed, [this]() {
        submit(m_input->text());
        m_input->clear();
    });
}

void TerminalPanel::submit(const QString& line)
{
    m_output->addItem("> " + line);
}

} // namespace cakery
