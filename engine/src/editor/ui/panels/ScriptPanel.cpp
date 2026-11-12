// do@Redlive

#include "ui/common/QtStringInterop.h"
#include "ui/common/EditorIcons.h"

#include "dopch.h"

#include "ScriptPanel.h"

#include "ui/script/CSharpCodeEditor.h"
#include "ui/script/CSharpHighlighter.h"
#include "ui/common/EditorWorkspaceContext.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QCompleter>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFontMetrics>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QStringListModel>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace cakery {

namespace {

class ScriptTabBar final : public QTabBar {
public:
    explicit ScriptTabBar(QWidget* parent) : QTabBar(parent) {}

protected:
    void tabInserted(int index) override
    {
        QTabBar::tabInserted(index);
        // Let the tab layout reserve space for the whole hit target, including
        // when tabs are elided, reordered or scrolled out of view.
        for (const auto side : {QTabBar::LeftSide, QTabBar::RightSide}) {
            if (QWidget* nativeClose = tabButton(index, side)) {
                setTabButton(index, side, nullptr);
                nativeClose->deleteLater();
            }
        }
        auto* close = new QToolButton(this);
        close->setObjectName(QStringLiteral("scriptTabClose"));
        close->setIcon(editorThemedIcon(QStringLiteral("x.svg")));
        close->setIconSize(QSize(10, 10));
        close->setFixedSize(18, 18);
        close->setAutoRaise(true);
        close->setFocusPolicy(Qt::NoFocus);
        close->setToolTip(tr("Close script"));
        close->setAccessibleName(tr("Close script"));
        setTabButton(index, QTabBar::RightSide, close);
        connect(close, &QToolButton::clicked, this, [this, close]() {
            for (int tab = 0; tab < count(); ++tab) {
                if (tabButton(tab, QTabBar::RightSide) == close) {
                    emit tabCloseRequested(tab);
                    return;
                }
            }
        });
    }
};

class ScriptTabs final : public QTabWidget {
public:
    explicit ScriptTabs(QWidget* parent) : QTabWidget(parent)
    {
        setTabBar(new ScriptTabBar(this));
    }
};

// Draw only the disclosure chevron. Native Windows branch painting also draws
// blue connector/focus fragments into the indentation area of selected rows.
class ScriptTreeWidget final : public QTreeWidget {
public:
    explicit ScriptTreeWidget(QWidget* parent) : QTreeWidget(parent)
    {
        setIndentation(18);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        header()->setStretchLastSection(true);
        header()->setSectionResizeMode(QHeaderView::Stretch);
        setTextElideMode(Qt::ElideMiddle);
    }

protected:
    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override
    {
        if (!model()->hasChildren(index)) return;
        const qreal x = isRightToLeft() ? rect.left() + indentation() / 2.0
                                       : rect.right() - indentation() / 2.0 + 1;
        const qreal y = rect.center().y();
        QPainterPath chevron;
        if (isExpanded(index)) {
            chevron.moveTo(x - 3, y - 1.5);
            chevron.lineTo(x, y + 1.5);
            chevron.lineTo(x + 3, y - 1.5);
        } else {
            chevron.moveTo(x - 1.5, y - 3);
            chevron.lineTo(x + 1.5, y);
            chevron.lineTo(x - 1.5, y + 3);
        }
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(palette().color(QPalette::PlaceholderText), 1.5,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawPath(chevron);
        painter->restore();
    }
};

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

constexpr int kWorkspacePathRole = Qt::UserRole;
constexpr int kWorkspaceDirectoryRole = Qt::UserRole + 1;

QString workspaceRoot(const EditorWorkspaceContext& context)
{
    const QString root = ToQString(context.session().assetRoot().string());
    return root.isEmpty() ? QString() : CSharpSymbolIndex::normalizePath(root);
}

bool isIdentifierCharacter(QChar character)
{
    return character.isLetterOrNumber() || character == QLatin1Char('_');
}

QString completionPrefix(const CSharpCodeEditor* editor)
{
    const QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) return {};
    const QString before = cursor.block().text().left(cursor.positionInBlock());
    int start = before.size();
    while (start > 0 && isIdentifierCharacter(before.at(start - 1))) --start;
    const QString prefix = before.mid(start);
    return !prefix.isEmpty() && prefix.front().isDigit() ? QString() : prefix;
}

} // namespace

ScriptPanel::ScriptPanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent), m_context(context)
{
    setObjectName(QStringLiteral("scriptPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    m_codeFontPointSize = m_context.config().scriptFontPointSize();
    auto& fontChanged = m_context.config().scriptFontChanged;
    m_fontSubscription = ScopedConnection(fontChanged, fontChanged.connect([this]() { applyCodeFont(); }));

    m_completionModel = new QStringListModel(this);
    m_completer = new QCompleter(m_completionModel, this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setMaxVisibleItems(8);
    auto* completionPopup = new QListView;
    completionPopup->setObjectName(QStringLiteral("scriptCompletionPopup"));
    completionPopup->setUniformItemSizes(true);
    completionPopup->setEditTriggers(QAbstractItemView::NoEditTriggers);
    completionPopup->setSelectionMode(QAbstractItemView::SingleSelection);
    completionPopup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    completionPopup->setMouseTracking(true);
    m_completer->setPopup(completionPopup);
    // Run before QCompleter forwards keys to the editor's indent/newline logic.
    completionPopup->installEventFilter(this);
    connect(m_completer, qOverload<const QString&>(&QCompleter::activated),
            this, &ScriptPanel::insertCompletion);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &ScriptPanel::onWatcherFileChanged);

    m_workspaceWatcher = new QFileSystemWatcher(this);
    connect(m_workspaceWatcher, &QFileSystemWatcher::directoryChanged,
            this, [this](const QString&) { scheduleWorkspaceRefresh(); });

    m_outlineTimer = new QTimer(this);
    m_outlineTimer->setSingleShot(true);
    m_outlineTimer->setInterval(500);
    connect(m_outlineTimer, &QTimer::timeout, this, &ScriptPanel::rebuildOutline);

    m_indexTimer = new QTimer(this);
    m_indexTimer->setSingleShot(true);
    m_indexTimer->setInterval(1500);
    connect(m_indexTimer, &QTimer::timeout, this, [this]() {
        const QString root = ToQString(m_context.session().assetRoot().string());
        if (!root.isEmpty()) {
            m_index.rebuild(root);
            rebuildCompletionModel();
        }
    });

    m_workspaceTimer = new QTimer(this);
    m_workspaceTimer->setSingleShot(true);
    m_workspaceTimer->setInterval(200);
    connect(m_workspaceTimer, &QTimer::timeout, this, [this]() {
        rebuildWorkspace();
        scheduleIndexRebuild();
    });

    m_assetSubscription = m_context.assets().subscribeChanged(
        [this]() { scheduleWorkspaceRefresh(); });

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("scriptSplitter"));
    splitter->setChildrenCollapsible(false);

    auto* sideSplitter = new QSplitter(Qt::Vertical, splitter);
    sideSplitter->setObjectName(QStringLiteral("scriptSideSplitter"));
    sideSplitter->setChildrenCollapsible(false);

    auto* workspaceBody = new QWidget(sideSplitter);
    workspaceBody->setObjectName(QStringLiteral("scriptWorkspaceBody"));
    workspaceBody->setAttribute(Qt::WA_StyledBackground, true);
    auto* workspaceLayout = new QVBoxLayout(workspaceBody);
    workspaceLayout->setContentsMargins(3, 3, 3, 3);
    workspaceLayout->setSpacing(3);
    auto* explorerTitle = new QLabel(tr("EXPLORER"), workspaceBody);
    explorerTitle->setObjectName(QStringLiteral("scriptSectionTitle"));
    workspaceLayout->addWidget(explorerTitle);
    m_workspaceFilter = new QLineEdit(workspaceBody);
    m_workspaceFilter->setObjectName(QStringLiteral("scriptWorkspaceFilter"));
    m_workspaceFilter->setPlaceholderText(tr("Filter scripts"));
    m_workspaceFilter->setClearButtonEnabled(true);
    auto* workspaceRefresh = new QToolButton(workspaceBody);
    workspaceRefresh->setIcon(editorThemedIcon(QStringLiteral("refresh-ccw.svg")));
    workspaceRefresh->setToolTip(tr("Refresh script explorer"));
    m_workspace = new ScriptTreeWidget(workspaceBody);
    m_workspace->setObjectName(QStringLiteral("scriptWorkspace"));
    m_workspace->setHeaderHidden(true);
    m_workspace->setRootIsDecorated(true);
    m_workspace->setUniformRowHeights(true);
    m_workspace->setIconSize(QSize(16, 16));
    m_workspace->setAnimated(true);
    m_workspace->setExpandsOnDoubleClick(true);
    m_workspace->setTextElideMode(Qt::ElideMiddle);
    m_workspace->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_workspaceFilter, &QLineEdit::textChanged,
            this, [this](const QString&) { applyWorkspaceFilter(); });
    connect(m_workspace, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int) {
        if (!item || item->data(0, kWorkspaceDirectoryRole).toBool()) {
            return;
        }
        const QString path = item->data(0, kWorkspacePathRole).toString();
        if (!path.isEmpty()) {
            openFile(path);
        }
    });
    connect(m_workspace, &QTreeWidget::itemClicked,
            this, [this](QTreeWidgetItem* item, int) {
        if (!item || item->data(0, kWorkspaceDirectoryRole).toBool()) {
            return;
        }
        const QString path = item->data(0, kWorkspacePathRole).toString();
        if (const int index = pageIndex(path); index >= 0) {
            m_tabs->setCurrentIndex(index);
        }
    });
    connect(workspaceRefresh, &QToolButton::clicked, this, [this]() {
        rebuildWorkspace();
        scheduleIndexRebuild();
    });
    auto* workspaceFilterRow = new QHBoxLayout();
    workspaceFilterRow->setContentsMargins(0, 0, 0, 0);
    workspaceFilterRow->setSpacing(4);
    workspaceFilterRow->addWidget(m_workspaceFilter, 1);
    workspaceFilterRow->addWidget(workspaceRefresh);
    workspaceLayout->addLayout(workspaceFilterRow);
    workspaceLayout->addWidget(m_workspace, 1);

    auto* outlineBody = new QWidget(sideSplitter);
    outlineBody->setObjectName(QStringLiteral("scriptOutlineBody"));
    outlineBody->setAttribute(Qt::WA_StyledBackground, true);
    auto* outlineLayout = new QVBoxLayout(outlineBody);
    outlineLayout->setContentsMargins(3, 3, 3, 3);
    outlineLayout->setSpacing(3);
    auto* outlineToggle = new QToolButton(outlineBody);
    outlineToggle->setObjectName(QStringLiteral("scriptOutlineToggle"));
    outlineToggle->setText(tr("SYMBOLS"));
    outlineToggle->setToolTip(tr("Classes, methods and fields in the current file. Click a symbol to jump to it."));
    outlineToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    outlineToggle->setCheckable(true);
    outlineToggle->setChecked(true);
    outlineToggle->setArrowType(Qt::DownArrow);
    outlineLayout->addWidget(outlineToggle);
    m_outlineFilter = new QLineEdit(outlineBody);
    m_outlineFilter->setObjectName(QStringLiteral("scriptOutlineFilter"));
    m_outlineFilter->setPlaceholderText(tr("Find symbol in file"));
    m_outlineFilter->setClearButtonEnabled(true);
    m_outline = new ScriptTreeWidget(outlineBody);
    m_outline->setObjectName(QStringLiteral("scriptOutline"));
    m_outline->setColumnCount(1);
    m_outline->setHeaderHidden(true);
    m_outline->setRootIsDecorated(true);
    m_outline->setUniformRowHeights(true);
    connect(outlineToggle, &QToolButton::toggled, this,
            [this, outlineToggle, outlineBody](bool expanded) {
        m_outlineFilter->setVisible(expanded);
        m_outline->setVisible(expanded);
        outlineToggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        outlineBody->setMaximumHeight(expanded ? QWIDGETSIZE_MAX : outlineBody->sizeHint().height());
    });
    connect(m_outline, &QTreeWidget::itemClicked,
            this, &ScriptPanel::jumpToOutlineItem);
    connect(m_outlineFilter, &QLineEdit::textChanged,
            this, [this](const QString&) { applyOutlineFilter(); });
    outlineLayout->addWidget(m_outlineFilter);
    outlineLayout->addWidget(m_outline, 1);

    sideSplitter->addWidget(workspaceBody);
    sideSplitter->addWidget(outlineBody);
    sideSplitter->setMinimumWidth(180);
    sideSplitter->setStretchFactor(0, 3);
    sideSplitter->setStretchFactor(1, 1);
    sideSplitter->setSizes({420, 220});

    auto* editorBody = new QWidget(splitter);
    editorBody->setMinimumWidth(300);
    auto* editorLayout = new QVBoxLayout(editorBody);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(2);

    auto* toolbar = new QWidget(editorBody);
    toolbar->setObjectName(QStringLiteral("scriptToolbar"));
    toolbar->setAttribute(Qt::WA_StyledBackground, true);
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(4, 2, 4, 2);
    toolbarLayout->setSpacing(3);
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
    findButton->setIcon(editorThemedIcon(QStringLiteral("search.svg")));
    findButton->setToolTip(tr("Find and replace (Ctrl+F)"));
    connect(findButton, &QToolButton::clicked, this, [this]() { showFindBar(false); });
    auto* backButton = new QToolButton(toolbar);
    backButton->setText(tr("Back"));
    backButton->setIcon(editorThemedIcon(QStringLiteral("arrow-left.svg")));
    backButton->setToolTip(tr("Navigate back (Alt+Left)"));
    connect(backButton, &QToolButton::clicked, this, [this]() { navigateBack(); });
    toolbarLayout->addWidget(saveButton);
    toolbarLayout->addWidget(saveAllButton);
    toolbarLayout->addWidget(findButton);
    toolbarLayout->addWidget(backButton);
    toolbarLayout->addStretch();

    m_tabs = new ScriptTabs(editorBody);
    m_tabs->setObjectName(QStringLiteral("scriptTabs"));
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    m_tabs->setIconSize(QSize(14, 14));
    connect(m_tabs, &QTabWidget::currentChanged,
            this, &ScriptPanel::setCurrentTab);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &ScriptPanel::closeTab);
    if (QTabBar* tabBar = m_tabs->findChild<QTabBar*>()) {
        tabBar->setExpanding(false);
        tabBar->setDrawBase(false);
        tabBar->setElideMode(Qt::ElideMiddle);
        tabBar->setUsesScrollButtons(true);
        connect(tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
            EditorPage page = m_pages.takeAt(from);
            m_pages.insert(to, page);
            setCurrentTab(m_tabs->currentIndex());
        });
    }

    m_statusLabel = new QLabel(editorBody);
    m_statusLabel->setObjectName(QStringLiteral("scriptStatus"));
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_statusLabel->installEventFilter(this);
    auto* statusBar = new QWidget(editorBody);
    statusBar->setObjectName(QStringLiteral("scriptStatusBar"));
    statusBar->setAttribute(Qt::WA_StyledBackground, true);
    auto* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(4, 1, 4, 1);
    statusLayout->setSpacing(6);
    m_saveStatus = new QLabel(statusBar);
    m_saveStatus->setObjectName(QStringLiteral("scriptSaveStatus"));
    m_reloadStatus = new QLabel(statusBar);
    m_reloadStatus->setObjectName(QStringLiteral("scriptReloadStatus"));
    statusLayout->addWidget(m_statusLabel, 1);
    statusLayout->addWidget(m_saveStatus);
    statusLayout->addWidget(m_reloadStatus);
    m_reloadSubscription = m_context.events().subscribe<ScriptReloadStatusEvent>(
        [this](const ScriptReloadStatusEvent& event) { updateScriptReloadStatus(event.state); });
    updateScriptReloadStatus(m_context.session().scriptReloadState());

    m_emptyEditor = new QWidget(editorBody);
    auto* emptyEditorLayout = new QVBoxLayout(m_emptyEditor);
    emptyEditorLayout->addStretch();
    auto* emptyEditorLabel = new QLabel(tr("Select a C# script from Explorer to open it here."),
                                        m_emptyEditor);
    emptyEditorLabel->setObjectName(QStringLiteral("scriptEmptyLabel"));
    emptyEditorLabel->setAlignment(Qt::AlignCenter);
    emptyEditorLayout->addWidget(emptyEditorLabel);
    emptyEditorLayout->addStretch();
    m_editorStack = new QStackedWidget(editorBody);
    m_editorStack->addWidget(m_emptyEditor);
    m_editorStack->addWidget(m_tabs);

    editorLayout->addWidget(toolbar);
    editorLayout->addWidget(m_editorStack, 1);
    editorLayout->addWidget(statusBar);

    splitter->addWidget(sideSplitter);
    splitter->addWidget(editorBody);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({220, 940});

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
    findNextButton->setIcon(editorThemedIcon(QStringLiteral("chevron-down.svg")));
    findNextButton->setToolTip(tr("Next match"));
    auto* findPrevButton = new QToolButton(m_findBar);
    findPrevButton->setIcon(editorThemedIcon(QStringLiteral("arrow-up.svg")));
    findPrevButton->setToolTip(tr("Previous match"));
    auto* closeFindButton = new QToolButton(m_findBar);
    closeFindButton->setIcon(editorThemedIcon(QStringLiteral("x.svg")));
    closeFindButton->setToolTip(tr("Close find"));
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

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);
    mainLayout->addWidget(splitter, 1);
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

    rebuildWorkspace();
    updateEmptyState();
}

bool ScriptPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress && m_completer->popup()->isVisible()
        && (watched == m_completer->popup() || watched == m_completer->widget())) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        auto* popup = m_completer->popup();
        if (keyEvent->key() == Qt::Key_Escape) {
            popup->hide();
            keyEvent->accept();
            return true;
        }
        if (!(keyEvent->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier
                                      | Qt::AltModifier | Qt::MetaModifier))) {
            switch (keyEvent->key()) {
            case Qt::Key_Tab:
            case Qt::Key_Return:
            case Qt::Key_Enter: {
                QModelIndex candidate = popup->currentIndex();
                if (!candidate.isValid()) candidate = m_completer->completionModel()->index(0, 0);
                insertCompletion(candidate.data(Qt::DisplayRole).toString());
                keyEvent->accept();
                return true;
            }
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown: {
                const int count = m_completer->completionCount();
                if (count > 0) {
                    const bool backwards = keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_PageUp;
                    const int step = keyEvent->key() == Qt::Key_PageUp || keyEvent->key() == Qt::Key_PageDown
                        ? m_completer->maxVisibleItems() : 1;
                    const int row = popup->currentIndex().isValid()
                        ? qBound(0, popup->currentIndex().row() + (backwards ? -step : step), count - 1) : 0;
                    const QModelIndex candidate = m_completer->completionModel()->index(row, 0);
                    popup->setCurrentIndex(candidate);
                    popup->scrollTo(candidate);
                }
                keyEvent->accept();
                return true;
            }
            default: break;
            }
        }
    }
    if (watched == m_statusLabel && event->type() == QEvent::Resize) updateStatus();
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
    const QString currentRoot = workspaceRoot(m_context);
    if (currentRoot != m_workspaceRoot) {
        rebuildWorkspace();
    }
    const QString normalized = CSharpSymbolIndex::normalizePath(path);
    const int existing = pageIndex(normalized);
    if (existing >= 0) {
        m_tabs->setCurrentIndex(existing);
        return;
    }

    QFile file(normalized);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_statusLabel->setText(tr("Could not open '%1'").arg(displayPath(normalized)));
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
    page.editor->setCodeFont(ToQString(m_context.config().scriptFontFamily()),
                             m_codeFontPointSize, m_context.config().scriptFontWeight());
    page.editor->setPlainText(content);
    page.editor->document()->setModified(false);
    page.highlighter = new CSharpHighlighter(page.editor->document());

    connect(page.editor, &QPlainTextEdit::textChanged, this, [this, editor = page.editor]() {
        if (editor == m_completer->widget()) updateCompletion();
    });
    connect(page.editor, &QPlainTextEdit::textChanged, this, [this]() { m_outlineTimer->start(); });
    connect(page.editor, &CSharpCodeEditor::cursorInfoChanged,
            this, [this](int, int) { updateStatus(); });
    connect(page.editor->document(), &QTextDocument::modificationChanged, this,
            [this, editor = page.editor](bool) {
        const int index = m_tabs->indexOf(editor);
        if (index >= 0) {
            updateTabText(index);
        }
        updateStatus();
        emit dirtyChanged();
    });
    connect(page.editor, &CSharpCodeEditor::navigateRequested,
            this, &ScriptPanel::navigateToSymbol);
    connect(page.editor, &CSharpCodeEditor::fontSizeChanged,
            this, &ScriptPanel::setCodeFontPointSize);

    m_watcher->addPath(normalized);

    const QString name = QFileInfo(normalized).fileName();
    m_pages.push_back(page);
    const int index = m_tabs->addTab(page.editor, editorAssetIcon({}, normalized), name);
    m_tabs->setTabToolTip(index, displayPath(normalized));

    updateEmptyState();
    m_tabs->setCurrentIndex(index);
    setCurrentTab(index);
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
            if (!saveAll()) return;
        } else if (box.clickedButton() == cancel || box.clickedButton() == nullptr) {
            return;
        } else if (box.clickedButton() == discard) {
            discardAll();
        }
    }
    closeAll(true);
    ensureIndexBuilt();
    rebuildCompletionModel();
    rebuildWorkspace();
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
                if (!savePage(i)) return;
            }
        }
        m_watcher->removePath(page.path);
        auto* editor = page.editor;
        m_pages.removeAt(i);
        m_tabs->removeTab(i);
        editor->deleteLater();
    }
    m_history.clear();
    m_historyPosition = -1;
    updateEmptyState();
    rebuildOutline();
    updateStatus();
    emit dirtyChanged();
}

bool ScriptPanel::savePage(int index)
{
    if (index < 0 || index >= m_pages.size()) {
        return false;
    }
    EditorPage& page = m_pages[index];
    const QString content = page.editor->toPlainText();

    ++m_suppressWatcher;
    m_watcher->removePath(page.path);

    QSaveFile file(page.path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        page.saveError = tr("Could not save %1: %2").arg(displayPath(page.path), file.errorString());
        m_saveStatus->setText(tr("Save failed"));
        m_saveStatus->setToolTip(page.saveError);
        m_watcher->addPath(page.path);
        --m_suppressWatcher;
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << content;
    stream.flush();
    if (stream.status() != QTextStream::Ok || !file.commit()) {
        page.saveError = tr("Could not save %1: %2").arg(displayPath(page.path), file.errorString());
        m_saveStatus->setText(tr("Save failed"));
        m_saveStatus->setToolTip(page.saveError);
        m_watcher->addPath(page.path);
        --m_suppressWatcher;
        return false;
    }

    page.saved = true;
    page.saveError.clear();
    page.editor->document()->setModified(false);
    m_watcher->addPath(page.path);
    --m_suppressWatcher;

    const int current = m_tabs->currentIndex();
    if (current == index) {
        updateStatus();
        m_outlineTimer->start();
    }
    scheduleIndexRebuild();
    updateStatus();
    m_context.session().requestScriptReload();
    return true;
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
    bool saved = true;
    for (int i = 0; i < m_pages.size(); ++i) {
        if (m_pages.at(i).editor->document()->isModified()) {
            saved = savePage(i) && saved;
        }
    }
    if (!saved) {
        m_saveStatus->setText(tr("Some files were not saved"));
        m_saveStatus->setToolTip(tr("Check the unsaved tabs for save errors."));
    }
    return saved;
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
        m_statusLabel->setText(tr("Could not reload '%1'").arg(displayPath(page.path)));
        return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QString content = stream.readAll();
    file.close();

    const bool wasCurrent = m_tabs->currentIndex() == index;
    page.saved = false;
    page.saveError.clear();
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
            if (!savePage(index)) return;
        }
    }
    m_watcher->removePath(page.path);
    auto* editor = page.editor;
    m_pages.removeAt(index);
    m_tabs->removeTab(index);
    editor->deleteLater();
    updateEmptyState();
    if (m_pages.isEmpty()) {
        rebuildOutline();
    } else {
        setCurrentTab(m_tabs->currentIndex());
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
        m_completer->setWidget(nullptr);
        rebuildOutline();
        updateStatus();
        return;
    }
    EditorPage& page = m_pages[index];
    m_completer->setWidget(page.editor);
    page.editor->installEventFilter(this);
    updateTabText(index);
    selectWorkspaceFile(page.path);
    rebuildOutline();
    updateStatus();
    page.editor->setFocus();
}

void ScriptPanel::ensureIndexBuilt()
{
    const QString root = ToQString(m_context.session().assetRoot().string());
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

void ScriptPanel::setCodeFontPointSize(int pointSize)
{
    const int normalized = qBound(8, pointSize, 32);
    m_context.config().setScriptFontPointSize(normalized);
}

void ScriptPanel::applyCodeFont()
{
    const auto& config = m_context.config();
    m_codeFontPointSize = config.scriptFontPointSize();
    for (EditorPage& page : m_pages) {
        if (page.editor) {
            page.editor->setCodeFont(ToQString(config.scriptFontFamily()),
                                     m_codeFontPointSize, config.scriptFontWeight());
        }
    }
}

QString ScriptPanel::displayPath(const QString& path) const
{
    if (!m_workspaceRoot.isEmpty()) {
        const QString relative = QDir(m_workspaceRoot).relativeFilePath(path);
        if (!QDir::isAbsolutePath(relative) && relative != QStringLiteral("..")
            && !relative.startsWith(QStringLiteral("../"))) {
            return relative == QStringLiteral(".") ? QStringLiteral("Assets")
                : QStringLiteral("Assets/") + relative;
        }
    }
    return QFileInfo(path).fileName();
}

void ScriptPanel::updateScriptReloadStatus(const String& state)
{
    QString text;
    if (state == "queued") text = tr("Update queued");
    else if (state == "deferred") text = tr("Update after Play stops");
    else if (state == "building") text = tr("Building scripts...");
    else if (state == "succeeded") text = tr("Scripts updated");
    else if (state == "unchanged") text = tr("Scripts up to date");
    else if (state == "failed") text = tr("Update failed (Console)");
    else if (state == "unavailable") text = tr("Runtime unavailable");
    else text = tr("Update idle");
    m_reloadStatus->setText(text);
    m_reloadStatus->setToolTip(text);
    m_reloadStatus->setProperty("failed", state == "failed" || state == "unavailable");
    m_reloadStatus->style()->unpolish(m_reloadStatus);
    m_reloadStatus->style()->polish(m_reloadStatus);
}

void ScriptPanel::rebuildWorkspace()
{
    if (!m_workspace) {
        return;
    }

    QStringList expandedDirectories;
    QString selectedPath;
    for (QTreeWidgetItemIterator it(m_workspace); *it; ++it) {
        QTreeWidgetItem* item = *it;
        if (item->data(0, kWorkspaceDirectoryRole).toBool() && item->isExpanded()) {
            expandedDirectories.append(item->data(0, kWorkspacePathRole).toString());
        }
        if (item == m_workspace->currentItem()) {
            selectedPath = item->data(0, kWorkspacePathRole).toString();
        }
    }

    const QSignalBlocker blocker(m_workspace);
    m_workspace->clear();
    if (m_workspaceWatcher) {
        const QStringList watched = m_workspaceWatcher->directories();
        if (!watched.isEmpty()) {
            m_workspaceWatcher->removePaths(watched);
        }
    }

    m_workspaceRoot = workspaceRoot(m_context);
    if (m_workspaceRoot.isEmpty() || !QDir(m_workspaceRoot).exists()) {
        auto* item = new QTreeWidgetItem(m_workspace);
        item->setText(0, tr("No project open"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        return;
    }

    const QFileInfo rootInfo(m_workspaceRoot);
    auto* rootItem = new QTreeWidgetItem(m_workspace);
    rootItem->setText(0, rootInfo.fileName().isEmpty() ? tr("Assets") : rootInfo.fileName());
    rootItem->setData(0, kWorkspacePathRole, m_workspaceRoot);
    rootItem->setData(0, kWorkspaceDirectoryRole, true);
    rootItem->setToolTip(0, displayPath(m_workspaceRoot));
    rootItem->setIcon(0, editorAssetIcon({}, m_workspaceRoot, true));

    QStringList watchedDirectories{m_workspaceRoot};
    const bool hasScripts = addWorkspaceChildren(rootItem, m_workspaceRoot, watchedDirectories);
    if (!hasScripts) {
        m_workspace->clear();
        auto* emptyItem = new QTreeWidgetItem(m_workspace);
        emptyItem->setText(0, tr("No C# scripts in Assets"));
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        if (m_workspaceWatcher) {
            const QStringList failed = m_workspaceWatcher->addPaths(watchedDirectories);
            if (!failed.isEmpty()) {
                m_statusLabel->setText(tr("Some script folders could not be watched"));
            }
        }
        return;
    }
    rootItem->setExpanded(true);

    for (QTreeWidgetItemIterator it(m_workspace); *it; ++it) {
        QTreeWidgetItem* item = *it;
        if (expandedDirectories.contains(item->data(0, kWorkspacePathRole).toString())) {
            item->setExpanded(true);
        }
    }

    if (m_workspaceWatcher) {
        const QStringList failed = m_workspaceWatcher->addPaths(watchedDirectories);
        if (!failed.isEmpty()) {
            m_statusLabel->setText(tr("Some script folders could not be watched"));
        }
    }

    applyWorkspaceFilter();
    if (selectedPath.isEmpty()) {
        if (const EditorPage* page = currentPage()) {
            selectedPath = page->path;
        }
    }
    if (!selectedPath.isEmpty()) {
        selectWorkspaceFile(selectedPath);
    }
}

void ScriptPanel::scheduleWorkspaceRefresh()
{
    if (m_workspaceTimer) {
        m_workspaceTimer->start();
    }
}

bool ScriptPanel::addWorkspaceChildren(QTreeWidgetItem* parent, const QString& directory,
                                       QStringList& watchedDirectories)
{
    bool hasScripts = false;
    const QDir dir(directory);
    const QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& entry : entries) {
        const QString path = CSharpSymbolIndex::normalizePath(entry.absoluteFilePath());
        if (entry.isDir()) {
            watchedDirectories.append(path);
            QTreeWidgetItem* item = nullptr;
            bool directoryHasScripts = false;
            if (!entry.isSymLink()) {
                item = new QTreeWidgetItem;
                directoryHasScripts = addWorkspaceChildren(item, path, watchedDirectories);
                if (!directoryHasScripts) {
                    delete item;
                    item = nullptr;
                }
            }
            if (item) {
                item->setText(0, entry.fileName());
                item->setData(0, kWorkspacePathRole, path);
                item->setData(0, kWorkspaceDirectoryRole, true);
                item->setToolTip(0, displayPath(path));
                item->setIcon(0, editorAssetIcon({}, path, true));
                parent->addChild(item);
                hasScripts = true;
            }
            continue;
        }
        if (!entry.isFile() || entry.suffix().compare(QStringLiteral("cs"), Qt::CaseInsensitive) != 0) {
            continue;
        }
        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, entry.fileName());
        item->setData(0, kWorkspacePathRole, path);
        item->setData(0, kWorkspaceDirectoryRole, false);
        item->setToolTip(0, displayPath(path));
        item->setIcon(0, editorAssetIcon({}, path));
        hasScripts = true;
    }
    return hasScripts;
}

void ScriptPanel::applyWorkspaceFilter()
{
    if (!m_workspace || m_workspace->topLevelItemCount() == 0) {
        return;
    }
    filterWorkspaceItem(m_workspace->topLevelItem(0),
                        m_workspaceFilter ? m_workspaceFilter->text().trimmed() : QString());
}

bool ScriptPanel::filterWorkspaceItem(QTreeWidgetItem* item, const QString& filter)
{
    if (!item) {
        return false;
    }
    bool childVisible = false;
    for (int i = 0; i < item->childCount(); ++i) {
        childVisible = filterWorkspaceItem(item->child(i), filter) || childVisible;
    }
    const bool isDirectory = item->data(0, kWorkspaceDirectoryRole).toBool();
    const bool matches = filter.isEmpty()
        || item->text(0).contains(filter, Qt::CaseInsensitive)
        || item->toolTip(0).contains(filter, Qt::CaseInsensitive);
    const bool visible = item->parent() == nullptr || (isDirectory ? matches || childVisible : matches);
    item->setHidden(!visible);
    if (isDirectory && childVisible && !filter.isEmpty()) {
        item->setExpanded(true);
    }
    return visible;
}

void ScriptPanel::selectWorkspaceFile(const QString& path)
{
    if (!m_workspace || path.isEmpty()) {
        return;
    }
    const QString normalized = CSharpSymbolIndex::normalizePath(path);
    const QSignalBlocker blocker(m_workspace);
    for (QTreeWidgetItemIterator it(m_workspace); *it; ++it) {
        QTreeWidgetItem* item = *it;
        if (item->data(0, kWorkspacePathRole).toString() == normalized) {
            m_workspace->setCurrentItem(item);
            m_workspace->scrollToItem(item);
            return;
        }
    }
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
        item->setForeground(0, kindColor(symbol.kind));
        item->setData(0, Qt::UserRole, symbol.line);
        item->setData(0, Qt::UserRole + 1, symbol.column);
        item->setToolTip(0, QStringLiteral("%1 %2\n%3:%4")
            .arg(kindLabel(symbol.kind), symbol.name, displayPath(page->path)).arg(symbol.line + 1));

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
                         bool recordHistory)
{
    if (recordHistory) {
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
    if (m_insertingCompletion) return;
    EditorPage* page = currentPage();
    if (!page || page->editor->isReadOnly()) {
        m_completer->popup()->hide();
        return;
    }
    if (!page->editor->hasFocus() && !m_completer->popup()->isVisible()) {
        return;
    }
    const QString word = completionPrefix(page->editor);
    if (word.length() < 2) {
        m_completer->popup()->hide();
        return;
    }
    m_completer->setCompletionPrefix(word);
    if (m_completer->completionCount() == 0) {
        m_completer->popup()->hide();
        return;
    }
    if (m_completer->completionCount() == 1 && m_completer->currentCompletion() == word) {
        m_completer->popup()->hide();
        return;
    }
    QRect rect = page->editor->cursorRect();
    rect.setWidth(qBound(220, m_completer->popup()->sizeHintForColumn(0)
        + m_completer->popup()->verticalScrollBar()->sizeHint().width() + 16, 420));
    m_completer->complete(rect);
    m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
}

void ScriptPanel::insertCompletion(const QString& completion)
{
    auto* editor = qobject_cast<CSharpCodeEditor*>(m_completer->widget());
    m_completer->popup()->hide();
    if (!editor || editor->isReadOnly() || completion.isEmpty()) {
        return;
    }
    const QString prefix = m_completer->completionPrefix();
    if (prefix.isEmpty() || completionPrefix(editor) != prefix) return;

    QScopedValueRollback<bool> inserting(m_insertingCompletion, true);
    QTextCursor cursor = editor->textCursor();
    const QString line = cursor.block().text();
    int end = cursor.positionInBlock();
    while (end < line.size() && isIdentifierCharacter(line.at(end))) ++end;
    const int start = cursor.position() - prefix.size();
    cursor.beginEditBlock();
    cursor.setPosition(cursor.block().position() + end);
    cursor.setPosition(start, QTextCursor::KeepAnchor);
    cursor.insertText(completion);
    cursor.endEditBlock();
    editor->setTextCursor(cursor);
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
        m_statusLabel->setToolTip(QString());
        m_saveStatus->clear();
        m_saveStatus->setToolTip(QString());
        return;
    }
    const int line = page->editor->currentLine() + 1;
    const int column = page->editor->currentColumn() + 1;
    const QString position = tr("  |  Ln %1, Col %2").arg(line).arg(column);
    const QFontMetrics metrics(m_statusLabel->font());
    const QString relative = displayPath(page->path);
    const QString elided = metrics.elidedText(relative, Qt::ElideMiddle,
        qMax(0, m_statusLabel->contentsRect().width() - metrics.horizontalAdvance(position) - 16));
    m_statusLabel->setText(elided + position);
    m_statusLabel->setToolTip(relative + position);
    m_saveStatus->setText(!page->saveError.isEmpty() ? tr("Save failed")
                         : page->editor->document()->isModified() ? tr("Unsaved")
                         : page->saved ? tr("Saved") : tr("Unmodified"));
    m_saveStatus->setToolTip(page->saveError.isEmpty() ? relative : page->saveError);
}

void ScriptPanel::onWatcherFileChanged(const QString& path)
{
    if (m_suppressWatcher > 0) {
        return;
    }
    scheduleWorkspaceRefresh();
    const int index = pageIndex(path);
    if (index < 0) {
        scheduleIndexRebuild();
        return;
    }
    EditorPage& page = m_pages[index];
    if (!QFileInfo::exists(page.path)) {
        m_statusLabel->setText(tr("'%1' was removed on disk")
            .arg(QFileInfo(path).fileName()));
        return;
    }
    m_watcher->removePath(page.path);
    m_watcher->addPath(page.path);
    if (page.editor->document()->isModified()) {
        m_statusLabel->setText(tr("'%1' changed on disk while there are unsaved edits")
            .arg(QFileInfo(path).fileName()));
        return;
    }
    QFile file(page.path);
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
    m_editorStack->setCurrentIndex(m_pages.isEmpty() ? 0 : 1);
    if (m_pages.isEmpty()) {
        hideFindBar();
    }
}

} // namespace cakery
