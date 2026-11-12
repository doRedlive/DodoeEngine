// do@Redlive

#include "dopch.h"

#include "ui/script/CSharpCodeEditor.h"

#include <QApplication>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QWheelEvent>

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

QChar matchingCloser(QChar character)
{
    switch (character.unicode()) {
    case '(': return QLatin1Char(')');
    case '[': return QLatin1Char(']');
    case '{': return QLatin1Char('}');
    case '\'': return QLatin1Char('\'');
    case '"': return QLatin1Char('"');
    default: return QChar();
    }
}

bool isClosingCharacter(QChar character)
{
    return character == QLatin1Char(')') || character == QLatin1Char(']')
        || character == QLatin1Char('}') || character == QLatin1Char('\'')
        || character == QLatin1Char('"');
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
    if (delta != 0) {
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
        selection.format.setBackground(QColor(0x34, 0x37, 0x46));
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
    painter.fillRect(event->rect(), QColor(0x28, 0x2a, 0x36));
    painter.setPen(QColor(0x62, 0x72, 0xa4));
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

int CSharpCodeEditor::codeFontPointSize() const
{
    return m_codeFontPointSize;
}

void CSharpCodeEditor::setCodeFontPointSize(int pointSize)
{
    setCodeFont(m_codeFontFamily, pointSize, m_codeFontWeight);
}

void CSharpCodeEditor::setCodeFont(const QString& family, int pointSize, int weight)
{
    m_codeFontFamily = family;
    m_codeFontPointSize = qBound(8, pointSize, 32);
    m_codeFontWeight = qBound(100, weight, 900);
    QFont codeFont(family, m_codeFontPointSize, m_codeFontWeight);
    codeFont.setStyleHint(QFont::Monospace);
    codeFont.setFixedPitch(true);
    // A widget-local rule wins over the application-wide pixel font, including
    // when a new page is polished or reparented into the tab widget.
    QString escapedFamily = family;
    escapedFamily.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escapedFamily.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    setStyleSheet(QStringLiteral("QPlainTextEdit#scriptEditor { font-family: \"%1\"; font-size: %2pt; font-weight: %3; }")
        .arg(escapedFamily).arg(m_codeFontPointSize).arg(m_codeFontWeight));
    setFont(codeFont);
    document()->setDefaultFont(codeFont);
    setTabStopDistance(m_indentWidth * QFontMetricsF(codeFont).horizontalAdvance(QLatin1Char(' ')));
    updateLineNumberAreaWidth();
    const QRect contents = contentsRect();
    m_lineNumberArea->setGeometry(contents.left(), contents.top(), lineNumberAreaWidth(), contents.height());
    m_lineNumberArea->update();
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
    if (isReadOnly()) {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }
    QTextCursor cursor = textCursor();

    const QString typedText = event->text();
    const Qt::KeyboardModifiers textModifiers = event->modifiers();
    const bool isDirectTextInput = typedText.size() == 1
        && !(textModifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    if (isDirectTextInput) {
        const QChar typed = typedText.at(0);
        if (!cursor.hasSelection() && isClosingCharacter(typed)
            && document()->characterAt(cursor.position()) == typed) {
            cursor.movePosition(QTextCursor::Right);
            setTextCursor(cursor);
            event->accept();
            return;
        }

        const QChar closer = matchingCloser(typed);
        if (!closer.isNull()) {
            cursor.beginEditBlock();
            if (cursor.hasSelection()) {
                const int start = cursor.selectionStart();
                QString selected = cursor.selectedText();
                selected.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
                cursor.insertText(QString(typed) + selected + closer);
                cursor.setPosition(start + 1);
                cursor.setPosition(start + 1 + selected.size(), QTextCursor::KeepAnchor);
            } else {
                cursor.insertText(QString(typed) + closer);
                cursor.movePosition(QTextCursor::Left);
            }
            cursor.endEditBlock();
            setTextCursor(cursor);
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_Backspace && event->modifiers() == Qt::NoModifier && !cursor.hasSelection()
        && cursor.position() > 0) {
        const QChar closer = matchingCloser(document()->characterAt(cursor.position() - 1));
        if (!closer.isNull() && closer == document()->characterAt(cursor.position())) {
            cursor.beginEditBlock();
            cursor.setPosition(cursor.position() - 1);
            cursor.setPosition(cursor.position() + 2, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.endEditBlock();
            setTextCursor(cursor);
            event->accept();
            return;
        }
    }

    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        cursor.beginEditBlock();
        if (cursor.hasSelection()) cursor.removeSelectedText();
        const QString blockText = cursor.block().text();
        const QString before = blockText.left(cursor.positionInBlock());
        const QString after = blockText.mid(cursor.positionInBlock());
        const QString indent = leadingWhitespace(before);
        const bool opensBrace = blockEndsWithOpenBrace(before.trimmed());
        const QString extra = opensBrace ? QString(m_indentWidth, QLatin1Char(' ')) : QString();
        const bool splitPair = !cursor.hasSelection() && opensBrace
            && after.trimmed().startsWith(QLatin1Char('}'));
        if (splitPair) {
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, leadingWhitespace(after).size());
        }
        cursor.insertText(QLatin1Char('\n') + indent + extra);
        const int bodyPosition = cursor.position();
        if (splitPair) cursor.insertText(QLatin1Char('\n') + indent);
        cursor.endEditBlock();
        cursor.setPosition(bodyPosition);
        setTextCursor(cursor);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Backtab
        || (event->key() == Qt::Key_Tab && event->modifiers() & Qt::ShiftModifier)) {
        QTextCursor anchor = cursor;
        const int startBlock = document()->findBlock(anchor.selectionStart()).blockNumber();
        const int endPosition = anchor.hasSelection() ? anchor.selectionEnd() - 1 : anchor.position();
        const int endBlock = document()->findBlock(endPosition).blockNumber();
        cursor.beginEditBlock();
        for (int b = startBlock; b <= endBlock; ++b) {
            QTextCursor line(document()->findBlockByNumber(b));
            const QString text = line.block().text();
            int remove = 0;
            while (remove < text.length() && remove < m_indentWidth
                && text.at(remove) == QLatin1Char(' ')) {
                ++remove;
            }
            if (text.startsWith(QLatin1Char('\t'))) remove = 1;
            if (remove > 0) {
                line.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, remove);
                line.removeSelectedText();
            }
        }
        cursor.endEditBlock();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Tab && event->modifiers() == Qt::NoModifier) {
        if (!cursor.hasSelection() && isClosingCharacter(document()->characterAt(cursor.position()))) {
            cursor.movePosition(QTextCursor::Right);
            setTextCursor(cursor);
            event->accept();
            return;
        }
        if (cursor.hasSelection()) {
            QTextCursor anchor = cursor;
            const int startBlock = document()->findBlock(anchor.selectionStart()).blockNumber();
            const int endBlock = document()->findBlock(anchor.selectionEnd() - 1).blockNumber();
            cursor.beginEditBlock();
            for (int b = startBlock; b <= endBlock; ++b) {
                QTextCursor line(document()->findBlockByNumber(b));
                line.insertText(QString(m_indentWidth, QLatin1Char(' ')));
            }
            cursor.endEditBlock();
        } else {
            const int spaces = m_indentWidth - cursor.positionInBlock() % m_indentWidth;
            cursor.insertText(QString(spaces, QLatin1Char(' ')));
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

void CSharpCodeEditor::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        int delta = event->angleDelta().y();
        if (delta == 0) {
            delta = event->pixelDelta().y();
        }
        if (delta != 0) {
            const int pointSize = qBound(8, codeFontPointSize() + (delta > 0 ? 1 : -1), 32);
            if (pointSize != codeFontPointSize()) {
                setCodeFontPointSize(pointSize);
                emit fontSizeChanged(pointSize);
            }
            event->accept();
            return;
        }
    }
    if (event->modifiers() & Qt::ShiftModifier) {
        QScrollBar* scrollBar = horizontalScrollBar();
        const QPoint pixels = event->pixelDelta();
        if (!pixels.isNull()) {
            const int delta = pixels.x() != 0 ? pixels.x() : pixels.y();
            scrollBar->setValue(scrollBar->value() - delta);
        } else {
            const QPoint angle = event->angleDelta();
            const int delta = angle.x() != 0 ? angle.x() : angle.y();
            // Forward as a horizontal wheel so Qt retains fractional wheel
            // steps and the system scroll speed. Shift must not turn it into
            // page scrolling inside QScrollBar.
            QWheelEvent horizontalEvent(
                scrollBar->mapFromGlobal(event->globalPosition().toPoint()),
                event->globalPosition(), QPoint(), QPoint(delta, 0),
                event->buttons(), event->modifiers() & ~Qt::ShiftModifier,
                event->phase(), event->inverted(), event->source());
            QApplication::sendEvent(scrollBar, &horizontalEvent);
        }
        event->accept();
        return;
    }
    QPlainTextEdit::wheelEvent(event);
}

} // namespace cakery
