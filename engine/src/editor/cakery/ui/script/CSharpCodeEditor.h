// do@Redlive

#pragma once

#include <QPlainTextEdit>

#include <QString>
#include <QWidget>

class QEvent;
class QMouseEvent;
class QResizeEvent;

namespace cakery {

class CSharpCodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CSharpCodeEditor(QWidget* parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent* event);
    int lineNumberAreaWidth() const;

    QString wordUnderCursor() const;
    int currentLine() const;
    int currentColumn() const;

    void gotoLine(int line, int column = 0, int selectLength = 0);

signals:
    void navigateRequested(const QString& symbol);
    void cursorInfoChanged(int line, int column);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect& rect, int delta);
    void updateCurrentLineHighlight();

private:
    QWidget* m_lineNumberArea = nullptr;
    int m_indentWidth = 4;
};

} // namespace cakery
