// do@Redlive

#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace cakery {

enum class CSharpSymbolKind {
    Namespace,
    Class,
    Struct,
    Interface,
    Enum,
    EnumMember,
    Method,
    Property,
    Field
};

struct CSharpSymbol {
    QString name;
    QString qualified;
    CSharpSymbolKind kind = CSharpSymbolKind::Class;
    int line = 0;
    int column = 0;
    QString filePath;
};

class CSharpSymbolIndex {
public:
    void rebuild(const QString& assetRoot);
    void clear();

    bool isBuilt() const { return m_built; }
    QString assetRoot() const { return m_assetRoot; }
    int fileCount() const { return m_files.size(); }

    const QVector<CSharpSymbol>& fileSymbols(const QString& filePath) const;
    static QVector<CSharpSymbol> parseContent(const QString& filePath,
                                              const QString& content);
    bool resolveDefinition(const QString& name, const QString& currentFile,
                           CSharpSymbol& out) const;
    QString findFileBySymbolName(const QString& name) const;
    QStringList completionWords(const QString& prefix, int maxResults) const;
    const QSet<QString>& allWords() const { return m_words; }

    static QString normalizePath(const QString& path);

private:
    struct FileIndex {
        QString filePath;
        QVector<CSharpSymbol> symbols;
    };

    void parseFile(const QString& filePath, FileIndex& out);
    static void parseLines(const QString& filePath, const QStringList& lines,
                           QVector<CSharpSymbol>& outSymbols, QSet<QString>* words);

    QVector<FileIndex> m_files;
    QHash<QString, int> m_fileLookup;
    QSet<QString> m_words;
    QMultiHash<QString, int> m_symbolLookup;
    QString m_assetRoot;
    bool m_built = false;
};

} // namespace cakery
