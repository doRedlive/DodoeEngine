// do@Redlive

#pragma once

#include <QSyntaxHighlighter>

#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QTextCharFormat>
#include <QVector>

class QTextDocument;

namespace cakery {

class CSharpHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    enum class Token {
        Plain,
        Comment,
        String,
        Number,
        Keyword,
        Type,
        Literal,
        Preprocessor,
        Attribute,
        TypeName
    };

    explicit CSharpHighlighter(QTextDocument* document);

protected:
    void highlightBlock(const QString& text) override;

private:
    enum BlockState {
        StateNormal = 0,
        StateBlockComment = 1,
        StateVerbatimString = 2
    };

    void formatRange(int start, int length, Token token);

    QHash<QString, Token> m_keywords;
    QTextCharFormat m_formats[10];
    QRegularExpression m_attributeRegex;
    QRegularExpression m_preprocessorRegex;
};

} // namespace cakery
