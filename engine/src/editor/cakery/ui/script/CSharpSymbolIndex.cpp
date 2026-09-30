// do@Redlive

#include "CSharpSymbolIndex.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

namespace cakery {

namespace {

const QSet<QString>& controlKeywords()
{
    static const QSet<QString> keywords = {
        QStringLiteral("if"), QStringLiteral("else"), QStringLiteral("while"),
        QStringLiteral("for"), QStringLiteral("foreach"), QStringLiteral("switch"),
        QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("using"),
        QStringLiteral("return"), QStringLiteral("new"), QStringLiteral("lock"),
        QStringLiteral("do"), QStringLiteral("try"), QStringLiteral("fixed"),
        QStringLiteral("yield"), QStringLiteral("await"), QStringLiteral("throw"),
        QStringLiteral("typeof"), QStringLiteral("sizeof"), QStringLiteral("nameof"),
        QStringLiteral("base"), QStringLiteral("this"), QStringLiteral("default"),
        QStringLiteral("checked"), QStringLiteral("unchecked"), QStringLiteral("when"),
    };
    return keywords;
}

const QSet<QString>& memberModifiers()
{
    static const QSet<QString> modifiers = {
        QStringLiteral("public"), QStringLiteral("private"), QStringLiteral("protected"),
        QStringLiteral("internal"), QStringLiteral("static"), QStringLiteral("virtual"),
        QStringLiteral("override"), QStringLiteral("sealed"), QStringLiteral("abstract"),
        QStringLiteral("async"), QStringLiteral("extern"), QStringLiteral("readonly"),
        QStringLiteral("const"), QStringLiteral("new"), QStringLiteral("unsafe"),
        QStringLiteral("volatile"), QStringLiteral("partial"), QStringLiteral("required"),
        QStringLiteral("file"), QStringLiteral("event"), QStringLiteral("fixed"),
        QStringLiteral("void"),
    };
    return modifiers;
}

bool shouldSkipDirectory(const QString& name)
{
    static const QSet<QString> skipped = {
        QStringLiteral("obj"), QStringLiteral("bin"), QStringLiteral(".git"),
        QStringLiteral("Library"), QStringLiteral("Temp"), QStringLiteral("Logs"),
        QStringLiteral("UserSettings"), QStringLiteral("Build"), QStringLiteral(".vs"),
        QStringLiteral("packages"), QStringLiteral(".idea"),
    };
    return skipped.contains(name);
}

QString stripCommentsAndStrings(const QString& line)
{
    QString result;
    result.reserve(line.length());
    bool inString = false;
    bool inChar = false;
    bool inLineComment = false;
    for (int i = 0; i < line.length(); ++i) {
        const QChar c = line.at(i);
        if (inLineComment) {
            result.append(QLatin1Char(' '));
            continue;
        }
        const bool escaped = i > 0 && line.at(i - 1) == QLatin1Char('\\');
        if (inString) {
            if (c == QLatin1Char('"') && !escaped) {
                inString = false;
            }
            result.append(QLatin1Char(' '));
            continue;
        }
        if (inChar) {
            if (c == QLatin1Char('\'') && !escaped) {
                inChar = false;
            }
            result.append(QLatin1Char(' '));
            continue;
        }
        if (c == QLatin1Char('/') && i + 1 < line.length()
            && line.at(i + 1) == QLatin1Char('/')) {
            inLineComment = true;
            result.append(QLatin1Char(' '));
            continue;
        }
        if (c == QLatin1Char('"')) {
            inString = true;
            result.append(QLatin1Char(' '));
            continue;
        }
        if (c == QLatin1Char('\'')) {
            inChar = true;
            result.append(QLatin1Char(' '));
            continue;
        }
        result.append(c);
    }
    return result;
}

struct MemberMatch {
    bool valid = false;
    bool isMethod = false;
    bool isProperty = false;
    bool isEnumMember = false;
    QString name;
};

MemberMatch matchMember(const QString& code, int braceDepth, bool inEnum)
{
    MemberMatch match;
    if (braceDepth < 1) {
        return match;
    }

    static const QRegularExpression memberRegex(QStringLiteral(
        "^[\\s\\[]*(?:\\[[^\\]]*\\]\\s*)*((?:[A-Za-z_]\\w*\\s+)*)"
        "([\\w<>\\[\\],.?]+\\s+)?([A-Za-z_]\\w*)\\s*(\\(|\\{|=>|;|=)"));
    const auto match2 = memberRegex.match(code);
    if (!match2.hasMatch()) {
        return match;
    }

    const QString modifierText = match2.captured(1);
    const QString typeText = match2.captured(2);
    const QString nameText = match2.captured(3);
    const QString terminator = match2.captured(4);

    if (nameText.isEmpty()) {
        return match;
    }

    static const QRegularExpression splitRegex(QStringLiteral("[\\s]+"));
    const QStringList modifiers = modifierText.split(splitRegex, Qt::SkipEmptyParts);
    for (const QString& word : modifiers) {
        if (!memberModifiers().contains(word)) {
            return match;
        }
    }
    if (controlKeywords().contains(nameText)) {
        return match;
    }

    const bool hasType = !typeText.trimmed().isEmpty();
    if (!hasType) {
        if (inEnum && (terminator == QLatin1Char(',') || terminator == QLatin1Char('{')
            || terminator == QLatin1Char(';') || terminator == QLatin1Char('='))) {
            match.valid = true;
            match.isEnumMember = true;
            match.name = nameText;
            return match;
        }
        return match;
    }

    if (terminator == QLatin1Char('(')) {
        match.valid = true;
        match.isMethod = true;
        match.name = nameText;
        return match;
    }

    const QStringList typeWords = typeText.trimmed().split(splitRegex, Qt::SkipEmptyParts);
    bool typeIsModifiers = !typeWords.isEmpty();
    for (const QString& word : typeWords) {
        if (!memberModifiers().contains(word)) {
            typeIsModifiers = false;
            break;
        }
    }
    if (typeIsModifiers) {
        if (inEnum) {
            match.valid = true;
            match.isEnumMember = true;
            match.name = nameText;
        }
        return match;
    }

    if (terminator == QLatin1Char('{') || terminator == QStringLiteral("=>")) {
        match.valid = true;
        match.isProperty = true;
        match.name = nameText;
        return match;
    }
    if (terminator == QLatin1Char(';') || terminator == QLatin1Char('=')) {
        match.valid = true;
        match.name = nameText;
        return match;
    }
    return match;
}

} // namespace

QString CSharpSymbolIndex::normalizePath(const QString& path)
{
    return QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
}

void CSharpSymbolIndex::rebuild(const QString& assetRoot)
{
    clear();
    m_assetRoot = normalizePath(assetRoot);
    if (m_assetRoot.isEmpty() || !QDir(m_assetRoot).exists()) {
        m_built = true;
        return;
    }

    QStringList files;
    QDirIterator iterator(m_assetRoot, QStringList{QStringLiteral("*.cs")},
                          QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString filePath = normalizePath(iterator.next());
        const QString relative = filePath.mid(m_assetRoot.length() + 1);
        const QStringList parts = relative.split(QLatin1Char('/'));
        bool skip = false;
        for (int i = 0; i < parts.size() - 1; ++i) {
            if (shouldSkipDirectory(parts.at(i))) {
                skip = true;
                break;
            }
        }
        if (!skip) {
            files.append(filePath);
        }
    }
    files.sort();

    m_files.reserve(files.size());
    for (const QString& filePath : files) {
        FileIndex fileIndex;
        parseFile(filePath, fileIndex);
        m_fileLookup.insert(fileIndex.filePath, m_files.size());
        m_files.push_back(fileIndex);
    }

    for (int filePos = 0; filePos < m_files.size(); ++filePos) {
        const FileIndex& fileIndex = m_files[filePos];
        for (const CSharpSymbol& symbol : fileIndex.symbols) {
            m_symbolLookup.insert(symbol.name, filePos);
        }
    }

    m_built = true;
}

void CSharpSymbolIndex::clear()
{
    m_files.clear();
    m_fileLookup.clear();
    m_words.clear();
    m_symbolLookup.clear();
    m_built = false;
}

void CSharpSymbolIndex::parseFile(const QString& filePath, FileIndex& out)
{
    QFile file(filePath);
    out.filePath = filePath;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QStringList lines;
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    while (!stream.atEnd()) {
        lines.append(stream.readLine());
    }
    parseLines(filePath, lines, out.symbols, &m_words);
}

QVector<CSharpSymbol> CSharpSymbolIndex::parseContent(const QString& filePath,
                                                      const QString& content)
{
    QVector<CSharpSymbol> symbols;
    const QStringList lines = content.split(QLatin1Char('\n'));
    parseLines(filePath, lines, symbols, nullptr);
    return symbols;
}

void CSharpSymbolIndex::parseLines(const QString& filePath, const QStringList& lines,
                                   QVector<CSharpSymbol>& outSymbols, QSet<QString>* words)
{
    static const QRegularExpression wordRegex(QStringLiteral("[A-Za-z_][A-Za-z0-9_]*"));
    static const QRegularExpression whitespaceRegex(QStringLiteral("\\S"));
    static const QRegularExpression namespaceRegex(
        QStringLiteral("^\\s*namespace\\s+([A-Za-z_]\\w*(?:\\.[A-Za-z_]\\w*)*)"));
    static const QRegularExpression typeRegex(
        QStringLiteral("\\b(class|struct|interface|enum)\\s+([A-Za-z_]\\w*)"));

    int braceDepth = 0;
    QVector<int> typeDepths;
    QStringList namespaceParts;
    QStringList typeStack;
    bool inEnum = false;
    QVector<bool> enumFlags;

    int lineNumber = 0;
    for (const QString& rawLine : lines) {
        QString line = stripCommentsAndStrings(rawLine);
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        const int column = line.indexOf(whitespaceRegex);
        const QString code = line.mid(qMax(0, column));

        auto typeIterator = typeRegex.globalMatch(code);
        while (typeIterator.hasNext()) {
            const auto typeMatch = typeIterator.next();
            const QString keyword = typeMatch.captured(1);
            const QString name = typeMatch.captured(2);

            CSharpSymbolKind kind = CSharpSymbolKind::Class;
            if (keyword == QStringLiteral("struct")) {
                kind = CSharpSymbolKind::Struct;
            } else if (keyword == QStringLiteral("interface")) {
                kind = CSharpSymbolKind::Interface;
            } else if (keyword == QStringLiteral("enum")) {
                kind = CSharpSymbolKind::Enum;
            }

            CSharpSymbol symbol;
            symbol.name = name;
            symbol.qualified = (namespaceParts.isEmpty() ? QString()
                : namespaceParts.join(QLatin1Char('.')) + QLatin1Char('.'));
            if (!typeStack.isEmpty()) {
                symbol.qualified += typeStack.join(QLatin1Char('.')) + QLatin1Char('.');
            }
            symbol.qualified += name;
            symbol.kind = kind;
            symbol.line = lineNumber;
            symbol.column = qMax(0, column) + typeMatch.capturedStart(2);
            symbol.filePath = filePath;
            outSymbols.push_back(symbol);

            typeStack.push_back(name);
            typeDepths.push_back(-1);
            inEnum = kind == CSharpSymbolKind::Enum;
            enumFlags.push_back(inEnum);

            if (words) {
                words->insert(name);
            }
        }

        for (const QChar& c : line) {
            if (c == QLatin1Char('{')) {
                ++braceDepth;
                for (int& depth : typeDepths) {
                    if (depth < 0) {
                        depth = braceDepth;
                        break;
                    }
                }
            } else if (c == QLatin1Char('}')) {
                --braceDepth;
            }
        }

        const auto namespaceMatch = namespaceRegex.match(code);
        if (namespaceMatch.hasMatch()) {
            namespaceParts = namespaceMatch.captured(1).split(QLatin1Char('.'));
            CSharpSymbol symbol;
            symbol.name = namespaceMatch.captured(1);
            symbol.qualified = symbol.name;
            symbol.kind = CSharpSymbolKind::Namespace;
            symbol.line = lineNumber;
            symbol.column = qMax(0, column);
            symbol.filePath = filePath;
            outSymbols.push_back(symbol);
            ++lineNumber;
            continue;
        }

        const int memberDepth = typeDepths.isEmpty() ? -1 : typeDepths.last();
        if (memberDepth > 0 && braceDepth == memberDepth) {
            const MemberMatch member = matchMember(code, braceDepth, inEnum);
            if (member.valid) {
                CSharpSymbol symbol;
                symbol.name = member.name;
                symbol.qualified = (namespaceParts.isEmpty() ? QString()
                    : namespaceParts.join(QLatin1Char('.')) + QLatin1Char('.'));
                if (!typeStack.isEmpty()) {
                    symbol.qualified += typeStack.join(QLatin1Char('.')) + QLatin1Char('.');
                }
                symbol.qualified += member.name;
                symbol.kind = member.isMethod ? CSharpSymbolKind::Method
                    : member.isEnumMember ? CSharpSymbolKind::EnumMember
                    : member.isProperty ? CSharpSymbolKind::Property
                    : CSharpSymbolKind::Field;
                symbol.line = lineNumber;
                symbol.column = qMax(0, column) + code.indexOf(member.name);
                symbol.filePath = filePath;
                outSymbols.push_back(symbol);
            }
        }

        while (!typeDepths.isEmpty() && braceDepth < typeDepths.last()) {
            typeDepths.removeLast();
            typeStack.removeLast();
            enumFlags.removeLast();
            inEnum = !enumFlags.isEmpty() && enumFlags.last();
        }

        if (words) {
            auto wordIterator = wordRegex.globalMatch(code);
            while (wordIterator.hasNext()) {
                const QString word = wordIterator.next().captured();
                if (word.length() >= 3) {
                    words->insert(word);
                }
            }
        }

        ++lineNumber;
    }
}

const QVector<CSharpSymbol>& CSharpSymbolIndex::fileSymbols(const QString& filePath) const
{
    static const QVector<CSharpSymbol> empty;
    const auto it = m_fileLookup.constFind(normalizePath(filePath));
    if (it == m_fileLookup.constEnd()) {
        return empty;
    }
    return m_files.at(it.value()).symbols;
}

bool CSharpSymbolIndex::resolveDefinition(const QString& name, const QString& currentFile,
                                          CSharpSymbol& out) const
{
    const QString normalizedCurrent = normalizePath(currentFile);

    const auto currentIt = m_fileLookup.constFind(normalizedCurrent);
    if (currentIt != m_fileLookup.constEnd()) {
        const QVector<CSharpSymbol>& symbols = m_files.at(currentIt.value()).symbols;
        for (const CSharpSymbol& symbol : symbols) {
            if (symbol.name == name) {
                out = symbol;
                return true;
            }
        }
    }

    const auto range = m_symbolLookup.equal_range(name);
    for (auto it = range.first; it != range.second; ++it) {
        const FileIndex& fileIndex = m_files.at(it.value());
        for (const CSharpSymbol& symbol : fileIndex.symbols) {
            if (symbol.name == name
                && (symbol.kind == CSharpSymbolKind::Class
                    || symbol.kind == CSharpSymbolKind::Struct
                    || symbol.kind == CSharpSymbolKind::Interface
                    || symbol.kind == CSharpSymbolKind::Enum
                    || symbol.kind == CSharpSymbolKind::Method)) {
                out = symbol;
                return true;
            }
        }
    }
    for (auto it = range.first; it != range.second; ++it) {
        const FileIndex& fileIndex = m_files.at(it.value());
        for (const CSharpSymbol& symbol : fileIndex.symbols) {
            if (symbol.name == name) {
                out = symbol;
                return true;
            }
        }
    }
    return false;
}

QString CSharpSymbolIndex::findFileBySymbolName(const QString& name) const
{
    CSharpSymbol symbol;
    if (resolveDefinition(name, QString(), symbol)) {
        return symbol.filePath;
    }
    return QString();
}

QStringList CSharpSymbolIndex::completionWords(const QString& prefix, int maxResults) const
{
    QStringList result;
    for (const QString& word : m_words) {
        if (prefix.isEmpty() || word.startsWith(prefix, Qt::CaseInsensitive)) {
            result.append(word);
        }
    }
    result.sort(Qt::CaseInsensitive);
    if (maxResults > 0 && result.size() > maxResults) {
        result.resize(maxResults);
    }
    return result;
}

} // namespace cakery
