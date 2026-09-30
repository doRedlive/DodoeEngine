// do@Redlive

#include "CSharpHighlighter.h"

#include <QColor>
#include <QFont>
#include <QTextDocument>

namespace cakery {

namespace {

const char* kKeywordList[] = {
    "abstract", "as", "async", "await", "base", "break", "case", "catch", "checked",
    "class", "const", "continue", "default", "delegate", "do", "else", "enum", "event",
    "explicit", "extern", "finally", "fixed", "for", "foreach", "get", "goto", "if",
    "implicit", "in", "interface", "internal", "is", "lock", "namespace", "new",
    "operator", "out", "override", "params", "partial", "private", "protected",
    "public", "readonly", "ref", "return", "sealed", "set", "sizeof", "stackalloc",
    "static", "struct", "switch", "this", "throw", "try", "typeof", "unchecked",
    "unsafe", "using", "virtual", "void", "volatile", "when", "where", "while", "yield",
    "add", "remove", "global", "nameof", "value", "init", "record", "with", "not",
    "and", "or", "extension", "file", "required", "scoped", "alias",
};

const char* kTypeList[] = {
    "bool", "byte", "char", "decimal", "double", "dynamic", "float", "int", "long",
    "object", "sbyte", "short", "string", "uint", "ulong", "ushort", "var", "nint",
    "nuint", "List", "Dictionary", "HashSet", "Queue", "Stack", "IEnumerable",
    "IList", "ICollection", "IDictionary", "Action", "Func", "Task", "Nullable",
};

const char* kLiteralList[] = {
    "true", "false", "null",
};

QTextCharFormat makeFormat(const QColor& color, bool bold = false, bool italic = false)
{
    QTextCharFormat format;
    format.setForeground(color);
    if (bold) {
        format.setFontWeight(QFont::Bold);
    }
    if (italic) {
        format.setFontItalic(true);
    }
    return format;
}

} // namespace

CSharpHighlighter::CSharpHighlighter(QTextDocument* document)
    : QSyntaxHighlighter(document)
{
    for (const char* word : kKeywordList) {
        m_keywords.insert(QString::fromLatin1(word), Token::Keyword);
    }
    for (const char* word : kTypeList) {
        m_keywords.insert(QString::fromLatin1(word), Token::Type);
    }
    for (const char* word : kLiteralList) {
        m_keywords.insert(QString::fromLatin1(word), Token::Literal);
    }

    m_formats[int(Token::Comment)] = makeFormat(QColor(0x6a, 0x99, 0x55), false, true);
    m_formats[int(Token::String)] = makeFormat(QColor(0xce, 0x91, 0x78));
    m_formats[int(Token::Number)] = makeFormat(QColor(0xb5, 0xce, 0xa8));
    m_formats[int(Token::Keyword)] = makeFormat(QColor(0xc5, 0x86, 0xc0));
    m_formats[int(Token::Type)] = makeFormat(QColor(0x4e, 0xc9, 0xb0));
    m_formats[int(Token::Literal)] = makeFormat(QColor(0x56, 0x9c, 0xd6));
    m_formats[int(Token::Preprocessor)] = makeFormat(QColor(0x9b, 0x9b, 0x9b));
    m_formats[int(Token::Attribute)] = makeFormat(QColor(0x4f, 0xc1, 0xff));
    m_formats[int(Token::TypeName)] = makeFormat(QColor(0x4e, 0xc9, 0xb0), true);

    m_attributeRegex = QRegularExpression(QStringLiteral(
        "\\[[A-Za-z_][\\w.]*(?:\\([^\\)\\n]*\\))?\\]"));
    m_preprocessorRegex = QRegularExpression(QStringLiteral("^\\s*#\\w+"));
}

void CSharpHighlighter::formatRange(int start, int length, Token token)
{
    if (length <= 0) {
        return;
    }
    setFormat(start, length, m_formats[int(token)]);
}

void CSharpHighlighter::highlightBlock(const QString& text)
{
    const int length = text.length();
    const int previousState = previousBlockState();
    int state = StateNormal;
    if (previousState == StateBlockComment || previousState == StateVerbatimString) {
        state = previousState;
    }

    if (state == StateNormal && length > 0) {
        const auto match = m_preprocessorRegex.match(text);
        if (match.hasMatch() && match.capturedStart() == 0) {
            formatRange(0, length, Token::Preprocessor);
            setCurrentBlockState(StateNormal);
            return;
        }
    }

    int index = 0;
    if (state == StateBlockComment) {
        const int end = text.indexOf(QStringLiteral("*/"));
        if (end < 0) {
            formatRange(0, length, Token::Comment);
            setCurrentBlockState(StateBlockComment);
            return;
        }
        formatRange(0, end + 2, Token::Comment);
        index = end + 2;
        state = StateNormal;
    } else if (state == StateVerbatimString) {
        int end = index;
        bool closed = false;
        while (end < length) {
            if (text.at(end) == QLatin1Char('"')) {
                if (end + 1 < length && text.at(end + 1) == QLatin1Char('"')) {
                    end += 2;
                    continue;
                }
                closed = true;
                break;
            }
            ++end;
        }
        if (!closed) {
            formatRange(0, length, Token::String);
            setCurrentBlockState(StateVerbatimString);
            return;
        }
        formatRange(0, end + 1, Token::String);
        index = end + 1;
        state = StateNormal;
    }

    while (index < length) {
        const QChar current = text.at(index);

        if (current == QLatin1Char('/') && index + 1 < length
            && text.at(index + 1) == QLatin1Char('/')) {
            formatRange(index, length - index, Token::Comment);
            break;
        }

        if (current == QLatin1Char('/') && index + 1 < length
            && text.at(index + 1) == QLatin1Char('*')) {
            int end = index + 2;
            int close = -1;
            while (end < length) {
                if (text.at(end) == QLatin1Char('*') && end + 1 < length
                    && text.at(end + 1) == QLatin1Char('/')) {
                    close = end + 1;
                    break;
                }
                ++end;
            }
            if (close < 0) {
                formatRange(index, length - index, Token::Comment);
                state = StateBlockComment;
                index = length;
                break;
            }
            formatRange(index, close + 1 - index, Token::Comment);
            index = close + 1;
            continue;
        }

        if (current == QLatin1Char('@') && index + 1 < length
            && text.at(index + 1) == QLatin1Char('"')) {
            int end = index + 2;
            bool closed = false;
            while (end < length) {
                if (text.at(end) == QLatin1Char('"')) {
                    if (end + 1 < length && text.at(end + 1) == QLatin1Char('"')) {
                        end += 2;
                        continue;
                    }
                    closed = true;
                    break;
                }
                ++end;
            }
            if (!closed) {
                formatRange(index, length - index, Token::String);
                state = StateVerbatimString;
                index = length;
                break;
            }
            formatRange(index, end + 1 - index, Token::String);
            index = end + 1;
            continue;
        }

        if (current == QLatin1Char('"') || (current == QLatin1Char('$')
            && index + 1 < length && text.at(index + 1) == QLatin1Char('"'))) {
            const int start = index;
            int end = current == QLatin1Char('$') ? index + 2 : index + 1;
            bool closed = false;
            while (end < length) {
                const QChar c = text.at(end);
                if (c == QLatin1Char('\\')) {
                    end += 2;
                    continue;
                }
                if (c == QLatin1Char('"')) {
                    closed = true;
                    break;
                }
                ++end;
            }
            if (!closed) {
                formatRange(start, length - start, Token::String);
                index = length;
                break;
            }
            formatRange(start, end + 1 - start, Token::String);
            index = end + 1;
            continue;
        }

        if (current == QLatin1Char('\'')) {
            const int start = index;
            int end = index + 1;
            bool closed = false;
            while (end < length) {
                const QChar c = text.at(end);
                if (c == QLatin1Char('\\')) {
                    end += 2;
                    continue;
                }
                if (c == QLatin1Char('\'')) {
                    closed = true;
                    break;
                }
                ++end;
            }
            if (closed && end < length) {
                formatRange(start, end + 1 - start, Token::String);
                index = end + 1;
                continue;
            }
            formatRange(start, 1, Token::Plain);
            index = start + 1;
            continue;
        }

        if (current.isDigit()) {
            int end = index;
            while (end < length && (text.at(end).isLetterOrNumber()
                || text.at(end) == QLatin1Char('_') || text.at(end) == QLatin1Char('.'))) {
                ++end;
            }
            formatRange(index, end - index, Token::Number);
            index = end;
            continue;
        }

        if (current.isLetter() || current == QLatin1Char('_')) {
            int end = index;
            while (end < length && (text.at(end).isLetterOrNumber()
                || text.at(end) == QLatin1Char('_'))) {
                ++end;
            }
            const QString word = text.mid(index, end - index);
            const auto it = m_keywords.constFind(word);
            if (it != m_keywords.constEnd()) {
                formatRange(index, end - index, it.value());
            } else if (!word.isEmpty() && word.at(0).isUpper()) {
                formatRange(index, end - index, Token::TypeName);
            }
            index = end;
            continue;
        }

        if (current == QLatin1Char('[')) {
            const auto match = m_attributeRegex.match(text, index);
            if (match.hasMatch() && match.capturedStart() == index) {
                formatRange(index, match.capturedLength(), Token::Attribute);
                index += match.capturedLength();
                continue;
            }
        }

        ++index;
    }

    setCurrentBlockState(state);
}

} // namespace cakery
