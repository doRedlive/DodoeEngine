// do@Redlive

#include "ScriptPanel.h"

#include "CSharpCodeEditor.h"
#include "CSharpHighlighter.h"
#include "cakery/ui/EditorWorkspaceContext.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QCompleter>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringListModel>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace cakery {

namespace {

QString kindLabel(CSharpSymbolKind kind)
{
    switch (kind) {
    case CSharpSymbolKind::Namespace: return QStringLiteral("namespace");
    case CSharpSymbolKind::Class: return QStringLiteral("class");
    case CSharpSymbolKind::Struct: return QStringLiteral("struct");
    case CSharpSymbolKind::Interface: return QStringLiteral("interface");
    case CSharpSymbolKind::Enum: return QStringLiteral("enum");
    case CSharpSymbolKind::EnumMember: return QStringLiteral("enum member");
    case CSharpSymbolKind::Method: return QStringLiteral("method");
    case CSharpSymbolKind::Property: return QStringLiteral("property");
    case CSharpSymbolKind::Field: return QStringLiteral("field");
    }
    return QString();
}

QColor kindColor(CSharpSymbolKind kind)
{
    switch (kind) {
    case CSharpSymbolKind::Namespace: return QColor(0x9b, 0x9b, 0x9b);
    case CSharpSymbolKind::Class: return QColor(0x4e, 0xc9, 0xb0);
    case CSharpSymbolKind::Struct: return QColor(0x4e, 0xc9, 0xb0);
    case CSharpSymbolKind::Interface: return QColor(0xb8, 0xd7, 0xa8);
    case CSharpSymbolKind::Enum: return QColor(0xb8, 0xd7, 0xa8);
    case CSharpSymbolKind::EnumMember: return QColor(0x86, 0xc6, 0x91);
    case CSharpSymbolKind::Method: return QColor(0xdc, 0xdc, 0xaa);
    case CSharpSymbolKind::Property: return QColor(0x9c, 0xdc, 0xfe);
    case CSharpSymbolKind::Field: return QColor(0x9c, 0xdc, 0xfe);
    }
    return QColor(0xd8, 0xd8, 0xd8);
}

QStringList engineApiWords()
{
    return {
        QStringLiteral("ICakeComponent"), QStringLiteral("ISystem"),
        QStringLiteral("Vector2"), QStringLiteral("Vector3"), QStringLiteral("Vector4"),
        QStringLiteral("Quaternion"), QStringLiteral("Color"), QStringLiteral("Debug"),
        QStringLiteral("Entity"), QStringLiteral("World"), QStringLiteral("Scene"),
        QStringLiteral("Coroutine"), QStringLiteral("AudioClip"), QStringLiteral("Time"),
        QStringLiteral("Input"), QStringLiteral("Mathf"),
    };
}

} // namespace

ScriptPanel::ScriptPanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent), m_context(context)
{
    setObjectName(QStringLiteral("scriptPanel"));

    m_completionModel = new QStringListModel(this);
    m_completer = new QCompleter(m_completionModel, this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setWidget(this);
    connect(m_completer, qOverload<const QString&>(&QCompleter::activated),
            this, &ScriptPanel::insertCompletion);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &ScriptPanel::onWatcherFileChanged);

    m_outlineTimer = new QTimer(this);
    m_outlineTimer->setSingleShot(true);
    m_outlineTimer->setInterval(500);
    connect(m_outlineTimer, &QTimer::timeout, this, &ScriptPanel::rebuildOutline);

    m_indexTimer = new QTimer(this);
    m_indexTimer->setSingleShot(true);
    m_indexTimer->setInterval(1500);
    connect(m_indexTimer, &QTimer::timeout, this, [this]() {
        const QString root = QString::fromStdString(m_context.session().assetRoot().string());
        if (!root.isEmpty()) {
            m_index.rebuild(root);
            rebuildCompletionModel();
        }
    });

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("scriptSplitter"));
    splitter->setChildrenCollapsible(false);

    auto* outlineBody = new QWidget(splitter);
    outlineBody->setObjectName(QStringLiteral("scriptOutlineBody"));
    auto* outlineLayout = new QVBoxLayout(outlineBody);
    outlineLayout->setContentsMargins(6, 6, 6, 6);
    outlineLayout->setSpacing(4);
    m_outlineFilter = new QLineEdit(outlineBody);
    m_outlineFilter->setObjectName(QStringLiteral("scriptOutlineFilter"));
    m_outlineFilter->setPlaceholderText(tr("Filter outline"));
    m_outlineFilter->setClearButtonEnabled(true);
    m_outline = new QTreeWidget(outlineBody);
    m_outline->setObjectName(QStringLiteral("scriptOutline"));
    m_outline->setColumnCount(2);
    m_outline->setHeaderHidden(true);
    m_outline->setRootIsDecorated(true);
    m_outline->setUniformRowHeights(true);
    m_outline->setColumnWidth(0, 190);
    connect(m_outline, &QTreeWidget::itemClicked,
            this, &ScriptPanel::jumpToOutlineItem);
    connect(m_outlineFilter, &QLineEdit::textChanged,
            this, [this](const QString&) { applyOutlineFilter(); });
    outlineLayout->addWidget(m_outlineFilter);
    outlineLayout->addWidget(m_outline, 1);

    auto* editorBody = new QWidget(splitter);
    auto* editorLayout = new QVBoxLayout(editorBody);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);

    auto* toolbar = new QWidget(editorBody);
    toolbar->setObjectName(QStringLiteral("scriptToolbar"));
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(6, 4, 6, 4);
    toolbarLayout->setSpacing(4);
    auto* saveButton = new QToolButton(toolbar);
    saveButton->setText(tr("Save"));
    saveButton->setToolTip(tr("Save current script (Ctrl+S)"));
    connect(saveButton, &QToolButton::clicked, this, [this]() { saveCurrent(); });
    auto* saveAllButton = new QToolButton(toolbar);
    saveAllButton->setText(tr("Save All"));
    saveAllButton->setToolTip(tr("Save all open scripts (Ctrl+Shift+S)"));
    connect(saveAllButton, &QToolButton::clicked, this, [this]() { saveAll(); });
    auto* findButton = new QToolButton(toolbar);
    findButton->setText(tr("Find"));
    findButton->setToolTip(tr("Find and replace (Ctrl+F)"));
    connect(findButton, &QToolButton::clicked, this, [this]() { showFindBar(false); });
    auto* backButton = new QToolButton(toolbar);
    backButton->setText(tr("Back"));
    backButton->setToolTip(tr("Navigate back (Alt+Left)"));
    connect(backButton, &QToolButton::clicked, this, [this]() { navigateBack(); });
    toolbarLayout->addWidget(saveButton);
    toolbarLayout->addWidget(saveAllButton);
    toolbarLayout->addWidget(findButton);
    toolbarLayout->addWidget(backButton);
    toolbarLayout->addStretch();

    m_tabs = new QTabWidget(editorBody);
    m_tabs->setObjectName(QStringLiteral("scriptTabs"));
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    connect(m_tabs, &QTabWidget::currentChanged,
            this, &ScriptPanel::setCurrentTab);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &ScriptPanel::closeTab);
    connect(m_tabs, &QTabWidget::tabMoved, this, [this](int from, int to) {
        EditorPage page = m_pages.takeAt(from);
        m_pages.insert(to, page);
    });

    m_statusLabel = new QLabel(editorBody);
    m_statusLabel->setObjectName(QStringLiteral("scriptStatus"));

    editorLayout->addWidget(toolbar);
    editorLayout->addWidget(m_tabs, 1);
    editorLayout->addWidget(m_statusLabel);

    splitter->addWidget(outlineBody);
    splitter->addWidget(editorBody);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({230, 900});

    m_findBar = new QWidget(this);
    m_findBar->setObjectName(QStringLiteral("scriptFindBar"));
    auto* findLayout = new QVBoxLayout(m_findBar);
    findLayout->setContentsMargins(6, 4, 6, 4);
    findLayout->setSpacing(4);
    auto* findRow = new QHBoxLayout();
    findRow->setSpacing(4);
    m_findEdit = new QLineEdit(m_findBar);
    m_findEdit->setPlaceholderText(tr("Find"));
    m_matchLabel = new QLabel(m_findBar);
    auto* findNextButton = new QToolButton(m_findBar);
    findNextButton->setText(tr("Next"));
    auto* findPrevButton = new QToolButton(m_findBar);
    findPrevButton->setText(tr("Previous"));
    auto* closeFindButton = new QToolButton(m_findBar);
    closeFindButton->setText(QStringLiteral("x"));
    findRow->addWidget(m_findEdit, 1);
    findRow->addWidget(m_matchLabel);
    findRow->addWidget(findPrevButton);
    findRow->addWidget(findNextButton);
    findRow->addWidget(closeFindButton);
    auto* replaceRow = new QWidget(m_findBar);
    auto* replaceLayout = new QHBoxLayout(replaceRow);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(4);
    m_replaceEdit = new QLineEdit(replaceRow);
    m_replaceEdit->setPlaceholderText(tr("Replace with"));
    m_caseSensitive = new QCheckBox(tr("Match case"), replaceRow);
    m_replaceButton = new QToolButton(replaceRow);
    m_replaceButton->setText(tr("Replace"));
    m_replaceAllButton = new QToolButton(replaceRow);
    m_replaceAllButton->setText(tr("Replace All"));
    replaceLayout->addWidget(m_replaceEdit, 1);
    replaceLayout->addWidget(m_caseSensitive);
    replaceLayout->addWidget(m_replaceButton);
    replaceLayout->addWidget(m_replaceAllButton);
    findLayout->addLayout(findRow);
    findLayout->addWidget(replaceRow);
    m_findBar->setVisible(false);

    connect(findNextButton, &QToolButton::clicked, this, [this]() { findNext(); });
    connect(findPrevButton, &QToolButton::clicked, this, [this]() { findPrevious(); });
    connect(closeFindButton, &QToolButton::clicked, this, [this]() { hideFindBar(); });
    connect(m_replaceButton, &QToolButton::clicked, this, [this]() { replaceCurrent(); });
    connect(m_replaceAllButton, &QToolButton::clicked, this, [this]() { replaceAllMatches(); });
    connect(m_findEdit, &QLineEdit::returnPressed, this, [this]() { findNext(); });
    connect(m_replaceEdit, &QLineEdit::returnPressed, this, [this]() { replaceCurrent(); });
    connect(m_findEdit, &QLineEdit::textChanged, this, [this](const QString&) {
        const int count = countMatches();
        m_matchLabel->setText(tr("%1 matches").arg(count));
    });
    m_findEdit->installEventFilter(this);
    m_replaceEdit->installEventFilter(this);

    auto* emptyBody = new QWidget(this);
    auto* emptyLayout = new QVBoxLayout(emptyBody);
    emptyLayout->addStretch();
    auto* emptyLabel = new QLabel(tr("Double-click a .cs script in the Project panel to open it here."), emptyBody);
    emptyLabel->setObjectName(QStringLiteral("scriptEmptyLabel"));
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyLabel);
    emptyLayout->addStretch();

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(emptyBody);
    m_stack->addWidget(splitter);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_stack, 1);
    mainLayout->addWidget(m_findBar);

    const auto addShortcut = [this](const QKeySequence& sequence, auto&& slot) {
        auto* shortcut = new QShortcut(sequence, this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, slot);
    };
    addShortcut(QKeySequence(QKeySequence::Save), [this]() { saveCurrent(); });
    addShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")), [this]() { saveAll(); });
    addShortcut(QKeySequence(QKeySequence::Find), [this]() { showFindBar(false); });
    addShortcut(QKeySequence(QStringLiteral("Ctrl+H")), [this]() { showFindBar(true); });
    addShortcut(QKeySequence(QStringLiteral("Ctrl+W")), [this]() { closeCurrentTab(); });
    addShortcut(QKeySequence(Qt::Key_F12), [this]() {
        if (const EditorPage* page = currentPage()) {
            navigateToSymbol(page->editor->wordUnderCursor());
        }
    });
    addShortcut(QKeySequence(QStringLiteral("Alt+Left")), [this]() { navigateBack(); });

    updateEmptyState();
}

bool ScriptPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress
        && (watched == m_findEdit || watched == m_replaceEdit)) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            hideFindBar();
            if (const EditorPage* page = currentPage()) {
                page->editor->setFocus();
            }
            return true;
        }
        if (watched == m_findEdit && keyEvent->key() == Qt::Key_Return
            && keyEvent->modifiers() & Qt::ShiftModifier) {
            findPrevious();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

int ScriptPanel::pageIndex(const QString& path) const
{
    const QString normalized = CSharpSymbolIndex::normalizePath(path);
    for (int i = 0; i < m_pages.size(); ++i) {
        if (m_pages.at(i).path == normalized) {
            return i;
        }
    }
    return -1;
}

ScriptPanel::EditorPage* ScriptPanel::currentPage()
{
    const int index = m_tabs->currentIndex();
    if (index < 0 || index >= m_pages.size()) {
        return nullptr;
    }
    return &m_pages[index];
}

const ScriptPanel::EditorPage* ScriptPanel::currentPage() const
{
    const int index = m_tabs->currentIndex();
    if (index < 0 || index >= m_pages.size()) {
        return nullptr;
    }
    return &m_pages.at(index);
}

void ScriptPanel::openFile(const QString& path)
{
    const QString normalized = CSharpSymbolIndex::normalizePath(path);
    const int existing = pageIndex(normalized);
    if (existing >= 0) {
        m_tabs->setCurrentIndex(existing);
        return;
    }

    QFile file(normalized);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_statusLabel->setText(tr("Could not open '%1'").arg(normalized));
        return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QString content = stream.readAll();
    file.close();

    ensureIndexBuilt();

    EditorPage page;
    page.path = normalized;
    page.editor = new CSharpCodeEditor(this);
    page.editor->setPlainText(content);
    page.editor->document()->setModified(false);
    page.highlighter = new CSharpHighlighter(page.editor->document());

    connect(page.editor, &QPlainTextEdit::textChanged,
            this, &ScriptPanel::updateCompletion);
    connect(page.editor, &CSharpCodeEditor::cursorInfoChanged,
            this, [this](int, int) { updateStatus(); });
    connect(page.editor->document(), &QTextDocument::modificationChanged, this,
            [this, editor = page.editor](bool) {
        const int index = m_tabs->indexOf(editor);
        if (index >= 0) {
            updateTabText(index);
        }
        emit dirtyChanged();
    });
    connect(page.editor, &CSharpCodeEditor::navigateRequested,
            this, &ScriptPanel::navigateToSymbol);

    m_watcher->addPath(normalized);

    const QString name = QFileInfo(normalized).fileName();
    const int index = m_tabs->addTab(page.editor, name);
    m_tabs->setTabToolTip(index, normalized);
    m_pages.push_back(page);

    updateEmptyState();
    m_tabs->setCurrentIndex(index);
}

void ScriptPanel::reopenProject()
{
    if (hasUnsavedChanges()) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(tr("Unsaved Scripts"));
        box.setText(tr("Some open scripts have unsaved changes."));
        QPushButton* save = box.addButton(tr("Save All"), QMessageBox::AcceptRole);
        QPushButton* discard = box.addButton(tr("Discard"), QMessageBox::DestructiveRole);
        QPushButton* cancel = box.addButton(tr("Cancel"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == save) {
            saveAll();
        } else if (box.clickedButton() == cancel || box.clickedButton() == nullptr) {
            return;
        } else if (box.clickedButton() == discard) {
            discardAll();
        }
    }
    closeAll(true);
    ensureIndexBuilt();
    rebuildCompletionModel();
}

bool ScriptPanel::hasUnsavedChanges() const
{
    for (const EditorPage& page : m_pages) {
        if (page.editor && page.editor->document()->isModified()) {
            return true;
        }
    }
    return false;
}

void ScriptPanel::closeAll(bool force)
{
    for (int i = m_pages.size() - 1; i >= 0; --i) {
        EditorPage& page = m_pages[i];
        if (!force && page.editor->document()->isModified()) {
            const int choice = QMessageBox::question(
                this, tr("Close Script"),
                tr("'%1' has unsaved changes. Close without saving?")
                    .arg(QFileInfo(page.path).fileName()),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            if (choice == QMessageBox::Cancel) {
                return;
            }
            if (choice == QMessageBox::Save) {
                savePage(i);
            }
        }
        m_watcher->removePath(page.path);
        m_tabs->removeTab(i);
        page.editor->deleteLater();
        m_pages.removeAt(i);
    }
    m_history.clear();
    m_historyPosition = -1;
    updateEmptyState();
    rebuildOutline();
    updateStatus();
    emit dirtyChanged();
}

void ScriptPanel::savePage(int index)
{
    if (index < 0 || index >= m_pages.size()) {
        return;
    }
    EditorPage& page = m_pages[index];
    const QString content = page.editor->toPlainText();

    ++m_suppressWatcher;
    m_watcher->removePath(page.path);

    QSaveFile file(page.path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_statusLabel->setText(tr("Could not save '%1'").arg(page.path));
        m_watcher->addPath(page.path);
        --m_suppressWatcher;
        return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << content;
    stream.flush();
    if (!file.commit()) {
        m_statusLabel->setText(tr("Could not save '%1'").arg(page.path));
        m_watcher->addPath(page.path);
        --m_suppressWatcher;
        return;
    }

    page.editor->document()->setModified(false);
    m_watcher->addPath(page.path);
    --m_suppressWatcher;

    const int current = m_tabs->currentIndex();
    if (current == index) {
        updateStatus();
        m_outlineTimer->start();
    }
    scheduleIndexRebuild();
}

void ScriptPanel::saveCurrent()
{
    const int index = m_tabs->currentIndex();
    if (index >= 0) {
        savePage(index);
    }
}

bool ScriptPanel::saveAll()
{
    for (int i = 0; i < m_pages.size(); ++i) {
        if (m_pages.at(i).editor->document()->isModified()) {
            savePage(i);
        }
    }
    return true;
}

void ScriptPanel::discardAll()
{
    for (EditorPage& page : m_pages) {
        page.editor->document()->setModified(false);
    }
    updateTabText(m_tabs->currentIndex());
    emit dirtyChanged();
}

void ScriptPanel::reloadPage(int index)
{
    if (index < 0 || index >= m_pages.size()) {
        return;
    }
    EditorPage& page = m_pages[index];
    QFile file(page.path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_statusLabel->setText(tr("Could not reload '%1'").arg(page.path));
        return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QString content = stream.readAll();
    file.close();

    const bool wasCurrent = m_tabs->currentIndex() == index;
    page.editor->setPlainText(content);
    page.editor->document()->setModified(false);
    if (wasCurrent) {
        rebuildOutline();
        updateStatus();
    }
}

void ScriptPanel::closeTab(int index)
{
    if (index < 0 || index >= m_pages.size()) {
        return;
    }
    EditorPage& page = m_pages[index];
    if (page.editor->document()->isModified()) {
        const int choice = QMessageBox::question(
            this, tr("Close Script"),
            tr("'%1' has unsaved changes. Close without saving?")
                .arg(QFileInfo(page.path).fileName()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (choice == QMessageBox::Cancel) {
            return;
        }
        if (choice == QMessageBox::Save) {
            savePage(index);
        }
    }
    m_watcher->removePath(page.path);
    m_tabs->removeTab(index);
    page.editor->deleteLater();
    m_pages.removeAt(index);
    updateEmptyState();
    if (m_pages.isEmpty()) {
        rebuildOutline();
    }
    emit dirtyChanged();
}

void ScriptPanel::closeCurrentTab()
{
    closeTab(m_tabs->currentIndex());
}

void ScriptPanel::updateTabText(int index)
{
    if (index < 0 || index >= m_pages.size()) {
        return;
    }
    const EditorPage& page = m_pages.at(index);
    QString name = QFileInfo(page.path).fileName();
    if (page.editor->document()->isModified()) {
        name += QStringLiteral(" *");
    }
    m_tabs->setTabText(index, name);
}

void ScriptPanel::setCurrentTab(int index)
{
    m_completer->popup()->hide();
    if (index < 0 || index >= m_pages.size()) {
        rebuildOutline();
        updateStatus();
        return;
    }
    EditorPage& page = m_pages[index];
    updateTabText(index);
    rebuildOutline();
    updateStatus();
    page.editor->setFocus();
}

void ScriptPanel::ensureIndexBuilt()
{
    const QString root = QString::fromStdString(m_context.session().assetRoot().string());
    const QString normalizedRoot = root.isEmpty() ? QString() : CSharpSymbolIndex::normalizePath(root);
    if (!m_index.isBuilt() || m_index.assetRoot() != normalizedRoot) {
        if (!normalizedRoot.isEmpty()) {
            m_index.rebuild(normalizedRoot);
        }
        rebuildCompletionModel();
    }
}

void ScriptPanel::scheduleIndexRebuild()
{
    m_indexTimer->start();
}

void ScriptPanel::rebuildOutline()
{
    m_outline->clear();
    const EditorPage* page = currentPage();
    if (!page) {
        return;
    }
    const QVector<CSharpSymbol> symbols = CSharpSymbolIndex::parseContent(
        page->path, page->editor->toPlainText());

    QTreeWidgetItem* lastType = nullptr;
    QTreeWidgetItem* lastNamespace = nullptr;
    for (const CSharpSymbol& symbol : symbols) {
        auto* item = new QTreeWidgetItem;
        item->setText(0, symbol.name);
        item->setText(1, kindLabel(symbol.kind));
        item->setForeground(0, kindColor(symbol.kind));
        item->setForeground(1, QColor(0x8a, 0x8a, 0x8a));
        item->setData(0, Qt::UserRole, symbol.line);
        item->setData(0, Qt::UserRole + 1, symbol.column);
        item->setToolTip(0, QStringLiteral("%1:%2")
            .arg(QFileInfo(page->path).fileName()).arg(symbol.line + 1));

        if (symbol.kind == CSharpSymbolKind::Namespace) {
            m_outline->addTopLevelItem(item);
            item->setExpanded(true);
            lastNamespace = item;
            lastType = nullptr;
        } else if (symbol.kind == CSharpSymbolKind::Class
            || symbol.kind == CSharpSymbolKind::Struct
            || symbol.kind == CSharpSymbolKind::Interface
            || symbol.kind == CSharpSymbolKind::Enum) {
            if (lastNamespace && !lastNamespace->parent()) {
                lastNamespace->addChild(item);
            } else {
                m_outline->addTopLevelItem(item);
            }
            item->setExpanded(true);
            lastType = item;
        } else {
            if (lastType) {
                lastType->addChild(item);
            } else if (lastNamespace && !lastNamespace->parent()) {
                lastNamespace->addChild(item);
            } else {
                m_outline->addTopLevelItem(item);
            }
        }
    }
    applyOutlineFilter();
}

void ScriptPanel::applyOutlineFilter()
{
    const QString filter = m_outlineFilter->text().trimmed();
    const auto visit = [&](QTreeWidgetItem* item, auto&& visitRef) -> bool {
        bool childMatch = false;
        for (int i = 0; i < item->childCount(); ++i) {
            childMatch = visitRef(item->child(i), visitRef) || childMatch;
        }
        const bool selfMatch = filter.isEmpty()
            || item->text(0).contains(filter, Qt::CaseInsensitive);
        const bool visible = selfMatch || childMatch;
        item->setHidden(!visible);
        if (visible && childMatch) {
            item->setExpanded(true);
        }
        return visible;
    };
    for (int i = 0; i < m_outline->topLevelItemCount(); ++i) {
        visit(m_outline->topLevelItem(i), visit);
    }
}

void ScriptPanel::jumpToOutlineItem(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (!item) {
        return;
    }
    const bool okLine = item->data(0, Qt::UserRole).canConvert<int>();
    if (!okLine) {
        return;
    }
    const int line = item->data(0, Qt::UserRole).toInt();
    const int column0 = item->data(0, Qt::UserRole + 1).toInt();
    jumpTo(QString(), line, column0, 0, true);
}

void ScriptPanel::jumpTo(const QString& path, int line, int column, int selectLength,
                         bool pushHistory)
{
    if (pushHistory) {
        pushHistory();
    }
    QString target = path;
    if (!target.isEmpty()) {
        openFile(target);
    }
    EditorPage* page = currentPage();
    if (!page) {
        return;
    }
    page->editor->gotoLine(line, column, selectLength);
    updateStatus();
}

void ScriptPanel::pushHistory()
{
    EditorPage* page = currentPage();
    if (!page) {
        return;
    }
    const int line = page->editor->textCursor().blockNumber();
    while (m_history.size() > m_historyPosition + 1) {
        m_history.removeLast();
    }
    m_history.append({page->path, line});
    if (m_history.size() > 64) {
        m_history.removeFirst();
    }
    m_historyPosition = m_history.size() - 1;
}

void ScriptPanel::navigateToSymbol(const QString& symbol)
{
    const QString name = symbol.trimmed();
    if (name.isEmpty()) {
        return;
    }
    EditorPage* page = currentPage();
    if (!page) {
        return;
    }
    ensureIndexBuilt();
    CSharpSymbol definition;
    if (!m_index.resolveDefinition(name, page->path, definition)) {
        m_statusLabel->setText(tr("No definition found for '%1'").arg(name));
        return;
    }
    jumpTo(definition.filePath, definition.line, definition.column,
           name.length(), true);
}

void ScriptPanel::navigateBack()
{
    if (m_historyPosition < 0 || m_history.isEmpty()) {
        return;
    }
    if (m_historyPosition >= m_history.size()) {
        m_historyPosition = m_history.size() - 1;
    }
    const auto entry = m_history.at(m_historyPosition);
    m_historyPosition = qMax(-1, m_historyPosition - 1);
    jumpTo(entry.first, entry.second, 0, 0, false);
}

void ScriptPanel::updateCompletion()
{
    EditorPage* page = currentPage();
    if (!page) {
        return;
    }
    if (m_completer->widget() != page->editor) {
        m_completer->setWidget(page->editor);
    }
    const QString word = page->editor->wordUnderCursor();
    if (word.length() < 2) {
        m_completer->popup()->hide();
        return;
    }
    m_completer->setCompletionPrefix(word);
    if (m_completer->completionCount() == 0) {
        m_completer->popup()->hide();
        return;
    }
    if (m_completer->popup()->isVisible()
        && m_completer->currentCompletion() == word) {
        return;
    }
    QRect rect = page->editor->cursorRect();
    rect.setWidth(m_completer->popup()->sizeHintForColumn(0)
        + m_completer->popup()->verticalScrollBar()->sizeHint().width() + 8);
    m_completer->complete(rect);
}

void ScriptPanel::insertCompletion(const QString& completion)
{
    auto* editor = qobject_cast<CSharpCodeEditor*>(m_completer->widget());
    if (!editor || completion.isEmpty()) {
        return;
    }
    QTextCursor cursor = editor->textCursor();
    const QString prefix = m_completer->completionPrefix();
    if (cursor.selectedText().isEmpty() && !prefix.isEmpty()) {
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, prefix.length());
    }
    cursor.insertText(completion);
    editor->setTextCursor(cursor);
    m_completer->popup()->hide();
}

void ScriptPanel::rebuildCompletionModel()
{
    QStringList words = engineApiWords();
    words.append(m_index.completionWords(QString(), 0));
    QSet<QString> unique(words.cbegin(), words.cend());
    QStringList list = unique.values();
    list.sort(Qt::CaseInsensitive);
    m_completionModel->setStringList(list);
}

void ScriptPanel::showFindBar(bool withReplace)
{
    m_findBar->setVisible(true);
    m_replaceEdit->parentWidget()->setVisible(withReplace);
    m_findEdit->setFocus();
    m_findEdit->selectAll();
    m_matchLabel->setText(tr("%1 matches").arg(countMatches()));
}

void ScriptPanel::hideFindBar()
{
    m_findBar->setVisible(false);
}

void ScriptPanel::findNext()
{
    search(false);
}

void ScriptPanel::findPrevious()
{
    search(true);
}

void ScriptPanel::search(bool backward)
{
    const EditorPage* page = currentPage();
    if (!page) {
        return;
    }
    const QString needle = m_findEdit->text();
    if (needle.isEmpty()) {
        m_matchLabel->setText(tr("%1 matches").arg(countMatches()));
        return;
    }
    QTextDocument::FindFlags flags;
    if (backward) {
        flags |= QTextDocument::FindBackward;
    }
    if (m_caseSensitive->isChecked()) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    QTextDocument* document = page->editor->document();
    QTextCursor found = document->find(needle, page->editor->textCursor(), flags);
    if (found.isNull()) {
        QTextCursor wrap = page->editor->textCursor();
        wrap.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        found = document->find(needle, wrap, flags);
    }
    if (!found.isNull()) {
        page->editor->setTextCursor(found);
        page->editor->setFocus();
    }
    m_matchLabel->setText(tr("%1 matches").arg(countMatches()));
}

void ScriptPanel::replaceCurrent()
{
    EditorPage* page = currentPage();
    if (!page) {
        return;
    }
    const QString needle = m_findEdit->text();
    if (needle.isEmpty()) {
        return;
    }
    const QString replacement = m_replaceEdit->text();
    QTextCursor cursor = page->editor->textCursor();
    const bool caseSensitive = m_caseSensitive->isChecked();
    if (cursor.hasSelection()
        && (caseSensitive ? cursor.selectedText() == needle
                          : cursor.selectedText().compare(needle, Qt::CaseInsensitive) == 0)) {
        cursor.insertText(replacement);
    }
    findNext();
}

void ScriptPanel::replaceAllMatches()
{
    EditorPage* page = currentPage();
    if (!page) {
        return;
    }
    const QString needle = m_findEdit->text();
    if (needle.isEmpty()) {
        return;
    }
    const QString replacement = m_replaceEdit->text();
    QTextDocument::FindFlags flags;
    if (m_caseSensitive->isChecked()) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    QTextDocument* document = page->editor->document();
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    int count = 0;
    while (true) {
        QTextCursor found = document->find(needle, cursor, flags);
        if (found.isNull()) {
            break;
        }
        found.insertText(replacement);
        cursor = found;
        ++count;
    }
    cursor.endEditBlock();
    m_matchLabel->setText(tr("%1 replaced").arg(count));
}

int ScriptPanel::countMatches() const
{
    const EditorPage* page = currentPage();
    if (!page || m_findEdit->text().isEmpty()) {
        return 0;
    }
    const QString needle = m_findEdit->text();
    const QString text = page->editor->toPlainText();
    const Qt::CaseSensitivity sensitivity = m_caseSensitive->isChecked()
        ? Qt::CaseSensitive : Qt::CaseInsensitive;
    int count = 0;
    int position = 0;
    while ((position = text.indexOf(needle, position, sensitivity)) >= 0) {
        ++count;
        position += needle.length();
    }
    return count;
}

void ScriptPanel::updateStatus()
{
    const EditorPage* page = currentPage();
    if (!page) {
        m_statusLabel->setText(tr("%1 script(s) indexed")
            .arg(m_index.isBuilt() ? m_index.fileCount() : 0));
        return;
    }
    const int line = page->editor->currentLine() + 1;
    const int column = page->editor->currentColumn() + 1;
    const QString elided = QFontMetrics(m_statusLabel->font()).elidedText(
        page->path, Qt::ElideMiddle, 420);
    m_statusLabel->setText(QStringLiteral("%1  \u2014  Ln %2, Col %3")
        .arg(elided).arg(line).arg(column));
}

void ScriptPanel::onWatcherFileChanged(const QString& path)
{
    if (m_suppressWatcher > 0) {
        return;
    }
    const int index = pageIndex(path);
    if (index < 0) {
        scheduleIndexRebuild();
        return;
    }
    EditorPage& page = m_pages[index];
    if (page.editor->document()->isModified()) {
        m_statusLabel->setText(tr("'%1' changed on disk while there are unsaved edits")
            .arg(QFileInfo(path).fileName()));
        return;
    }
    QFile file(page.path);
    if (!file.exists()) {
        m_statusLabel->setText(tr("'%1' was removed on disk")
            .arg(QFileInfo(path).fileName()));
        return;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QString content = stream.readAll();
    file.close();
    if (content == page.editor->toPlainText()) {
        return;
    }
    reloadPage(index);
}

void ScriptPanel::updateEmptyState()
{
    m_stack->setCurrentIndex(m_pages.isEmpty() ? 0 : 1);
}

} // namespace cakery
