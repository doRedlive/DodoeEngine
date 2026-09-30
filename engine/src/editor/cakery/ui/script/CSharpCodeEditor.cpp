// do@Redlive

#include "CSharpCodeEditor.h"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QMouseEvent>
#include <QTextBlock>
#include <QTextCursor>

namespace cakery {

namespace {

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(CSharpCodeEditor* editor)
        : QWidget(editor), m_editor(editor) {}

    QSize sizeHint() const override { return QSize(m_editor->lineNumberAreaWidth(), 0); }

protected:
    void paintEvent(QPaintEvent* event) override { m_editor->lineNumberAreaPaintEvent(event); }

private:
    CSharpCodeEditor* m_editor = nullptr;
};

QString leadingWhitespace(const QString& text)
{
    int end = 0;
    while (end < text.length()
        && (text.at(end) == QLatin1Char(' ') || text.at(end) == QLatin1Char('\t'))) {
        ++end;
    }
    return text.left(end);
}

QString cleanWord(QString text)
{
    static const QString wordChars = QStringLiteral("_0123456789");
    while (!text.isEmpty() && !(text.front().isLetter() || text.front() == QLatin1Char('_'))) {
        text.removeFirst();
    }
    while (!text.isEmpty()
        && !(text.back().isLetter() || text.back().isDigit() || text.back() == QLatin1Char('_'))) {
        text.removeLast();
    }
    return text;
}

bool blockEndsWithOpenBrace(const QString& trimmed)
{
    return trimmed.endsWith(QLatin1Char('{'));
}

} // namespace

CSharpCodeEditor::CSharpCodeEditor(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setObjectName(QStringLiteral("scriptEditor"));
    QFont codeFont(QStringLiteral("Consolas"));
    codeFont.setStyleHint(QFont::Monospace);
    codeFont.setFixedPitch(true);
    codeFont.setPointSize(11);
    setFont(codeFont);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(4 * QFontMetricsF(font()).horizontalAdvance(QLatin1Char(' ')));
    setTabChangesFocus(false);
    setWordWrapMode(QTextOption::NoWrap);
    setCursorWidth(2);

    m_lineNumberArea = new LineNumberArea(this);
    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &CSharpCodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &CSharpCodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &CSharpCodeEditor::updateCurrentLineHighlight);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        emit cursorInfoChanged(currentLine(), currentColumn());
    });

    updateLineNumberAreaWidth();
    updateCurrentLineHighlight();
}

int CSharpCodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    const int width = 14 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return width;
}

void CSharpCodeEditor::updateLineNumberAreaWidth()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CSharpCodeEditor::updateLineNumberArea(const QRect& rect, int delta)
{
    if (delta > 0) {
        m_lineNumberArea->scroll(0, delta);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }
    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth();
    }
}

void CSharpCodeEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect contents = contentsRect();
    m_lineNumberArea->setGeometry(contents.left(), contents.top(),
                                  lineNumberAreaWidth(), contents.height());
}

void CSharpCodeEditor::updateCurrentLineHighlight()
{
    QList<QTextEdit::ExtraSelection> selections;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor(0x2a, 0x2a, 0x2e));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.append(selection);
    }
    setExtraSelections(selections);
}

void CSharpCodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor(0x1e, 0x1e, 0x22));
    painter.setPen(QColor(0x6a, 0x6a, 0x70));
    painter.setFont(font());

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const QString number = QString::number(blockNumber + 1);
            painter.drawText(0, top, m_lineNumberArea->width() - 7,
                             fontMetrics().height(), Qt::AlignRight, number);
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

QString CSharpCodeEditor::wordUnderCursor() const
{
    QTextCursor word = textCursor();
    word.select(QTextCursor::WordUnderCursor);
    return cleanWord(word.selectedText());
}

int CSharpCodeEditor::currentLine() const
{
    return textCursor().blockNumber();
}

int CSharpCodeEditor::currentColumn() const
{
    return textCursor().positionInBlock();
}

void CSharpCodeEditor::gotoLine(int line, int column, int selectLength)
{
    line = qMax(0, line);
    QTextBlock block = document()->findBlockByNumber(line);
    if (!block.isValid()) {
        return;
    }
    QTextCursor cursor(block);
    const int position = block.position() + qBound(0, column, block.length() - 1);
    cursor.setPosition(position);
    if (selectLength > 0) {
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, selectLength);
    }
    setTextCursor(cursor);
    centerCursor();
    setFocus();
}

void CSharpCodeEditor::keyPressEvent(QKeyEvent* event)
{
    QTextCursor cursor = textCursor();

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        const QString blockText = cursor.block().text();
        const QString indent = leadingWhitespace(blockText);
        QString extra;
        const QString trimmed = blockText.trimmed();
        bool closesBrace = false;
        if (blockEndsWithOpenBrace(trimmed)) {
            extra = QString(m_indentWidth, QLatin1Char(' '));
            const QString rest = toPlainText().mid(cursor.position());
            closesBrace = rest.trimmed().startsWith(QLatin1Char('}'));
        }
        cursor.insertText(QLatin1Char('\n') + indent + extra);
        if (closesBrace) {
            cursor.insertText(QLatin1Char('\n') + indent);
        }
        setTextCursor(cursor);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Backtab
        || (event->key() == Qt::Key_Tab && event->modifiers() & Qt::ShiftModifier)) {
        QTextCursor anchor = cursor;
        const int startBlock = document()->findBlock(anchor.selectionStart()).blockNumber();
        const int endBlock = document()->findBlock(anchor.selectionEnd()).blockNumber();
        for (int b = startBlock; b <= endBlock; ++b) {
            QTextCursor line(document()->findBlockByNumber(b));
            line.beginEditBlock();
            const QString text = line.block().text();
            int remove = 0;
            while (remove < text.length() && remove < m_indentWidth
                && text.at(remove) == QLatin1Char(' ')) {
                ++remove;
            }
            if (remove > 0) {
                line.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, remove);
                line.removeSelectedText();
            }
            line.endEditBlock();
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Tab) {
        if (cursor.hasSelection()) {
            QTextCursor anchor = cursor;
            const int startBlock = document()->findBlock(anchor.selectionStart()).blockNumber();
            const int endBlock = document()->findBlock(anchor.selectionEnd()).blockNumber();
            for (int b = startBlock; b <= endBlock; ++b) {
                QTextCursor line(document()->findBlockByNumber(b));
                line.beginEditBlock();
                line.insertText(QString(m_indentWidth, QLatin1Char(' ')));
                line.endEditBlock();
            }
        } else {
            cursor.insertText(QString(m_indentWidth, QLatin1Char(' ')));
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_BraceRight) {
        const QString blockText = cursor.block().text();
        const QString indent = leadingWhitespace(blockText);
        const bool onEmptyIndentLine = !indent.isEmpty()
            && cursor.positionInBlock() == indent.length()
            && blockText.trimmed().isEmpty();
        if (onEmptyIndentLine) {
            const QString parentIndent = indent.left(
                qMax(0, indent.length() - m_indentWidth));
            QTextCursor line = cursor;
            line.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
            line.insertText(parentIndent + QLatin1Char('}'));
            setTextCursor(line);
            event->accept();
            return;
        }
    }

    QPlainTextEdit::keyPressEvent(event);
}

void CSharpCodeEditor::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton
        && event->modifiers() & Qt::ControlModifier) {
        QTextCursor clicked = cursorForPosition(event->pos());
        clicked.select(QTextCursor::WordUnderCursor);
        const QString word = cleanWord(clicked.selectedText());
        if (!word.isEmpty()) {
            emit navigateRequested(word);
            event->accept();
            return;
        }
    }
    QPlainTextEdit::mousePressEvent(event);
}

} // namespace cakery
