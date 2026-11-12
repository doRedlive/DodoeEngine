// do@Redlive
#pragma once
#include "dopch.h"

#include <QPlainTextEdit>

#include <QString>
#include <QWidget>

class QEvent;
class QMouseEvent;
class QResizeEvent;
class QWheelEvent;

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
    int codeFontPointSize() const;

    void gotoLine(int line, int column = 0, int selectLength = 0);
    void setCodeFontPointSize(int pointSize);
    void setCodeFont(const QString& family, int pointSize, int weight);

signals:
    void navigateRequested(const QString& symbol);
    void cursorInfoChanged(int line, int column);
    void fontSizeChanged(int pointSize);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect& rect, int delta);
    void updateCurrentLineHighlight();

private:
    QWidget* m_lineNumberArea = nullptr;
    int m_indentWidth = 4;
    QString m_codeFontFamily = QStringLiteral("Consolas");
    int m_codeFontPointSize = 11;
    int m_codeFontWeight = 400;
};

} // namespace cakery
