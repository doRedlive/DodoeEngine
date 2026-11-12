// do@Redlive

#include "dopch.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "FramelessChrome.h"

#include "ui/common/EditorIcons.h"

#include <FloatingDockContainer.h>

#include <QAbstractButton>
#include <QColorDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#include <vector>

namespace cakery {

FloatingWindowTitleBar::FloatingWindowTitleBar(QWidget* window, bool allowMaximize)
    : QWidget(window), m_window(window), m_allowMaximize(allowMaximize)
{
    setObjectName(QStringLiteral("editorTitleBar"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 2, 0, 2);
    layout->setSpacing(4);

    m_title = new QLabel(window->windowTitle(), this);
    m_title->setObjectName(QStringLiteral("editorWindowTitle"));
    m_title->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(m_title);
    layout->addStretch();

    auto addButton = [this, layout](const QString& objectName, const QString& icon,
                                    const QString& tooltip, auto callback) {
        auto* button = new QToolButton(this);
        button->setObjectName(objectName);
        button->setIcon(editorThemedIcon(icon));
        button->setIconSize(QSize(16, 16));
        button->setToolTip(tooltip);
        connect(button, &QToolButton::clicked, this, callback);
        layout->addWidget(button);
        return button;
    };

    if (m_allowMaximize) {
        addButton(QStringLiteral("windowMinButton"), QStringLiteral("minus.svg"),
                  tr("Minimize"), [this]() { m_window->showMinimized(); });
        m_maxButton = addButton(QStringLiteral("windowMaxButton"),
                                QStringLiteral("maximize-2.svg"), tr("Maximize"),
                                [this]() { toggleMaximize(); });
    }
    addButton(QStringLiteral("windowCloseButton"), QStringLiteral("x.svg"),
              tr("Close"), [this]() { m_window->close(); });

    connect(window, &QWidget::windowTitleChanged, this, [this](const QString& title) {
        m_title->setText(title);
    });
    connect(window, &QObject::destroyed, this, [this]() { m_window = nullptr; });
    updateMaximizeButton();
}

void FloatingWindowTitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_window && m_window->windowHandle()) {
        m_window->windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void FloatingWindowTitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_allowMaximize && event->button() == Qt::LeftButton) {
        toggleMaximize();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void FloatingWindowTitleBar::toggleMaximize()
{
    if (!m_window) {
        return;
    }
    if (m_window->windowState().testFlag(Qt::WindowMaximized) || m_window->isFullScreen()) {
        m_window->showNormal();
    } else {
        m_window->showMaximized();
    }
    updateMaximizeButton();
}

void FloatingWindowTitleBar::updateMaximizeButton()
{
    if (!m_maxButton || !m_window) {
        return;
    }
    const bool restore = m_window->isMaximized();
    m_maxButton->setIcon(editorThemedIcon(restore ? QStringLiteral("minimize-2.svg")
                                                  : QStringLiteral("maximize-2.svg")));
    m_maxButton->setToolTip(restore ? tr("Restore Down") : tr("Maximize"));
}

UnsavedChangesDialog::UnsavedChangesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Unsaved Changes"));
    setObjectName(QStringLiteral("unsavedChangesDialog"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setProperty("editorDialog", true);
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    setModal(true);
    setMinimumWidth(430);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(new FloatingWindowTitleBar(this, false));

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("unsavedChangesBody"));
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(20, 18, 20, 18);
    bodyLayout->setSpacing(16);

    auto* messageRow = new QHBoxLayout();
    messageRow->setSpacing(16);
    auto* icon = new QLabel(body);
    icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(QSize(42, 42)));
    icon->setFixedSize(42, 42);
    messageRow->addWidget(icon, 0, Qt::AlignTop);
    auto* message = new QLabel(tr("The current scene has unsaved changes."), body);
    message->setObjectName(QStringLiteral("unsavedChangesMessage"));
    message->setWordWrap(true);
    messageRow->addWidget(message, 1, Qt::AlignVCenter);
    bodyLayout->addLayout(messageRow);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto addButton = [this, buttons](const QString& text, QMessageBox::StandardButton result) {
        auto* button = new QPushButton(text, this);
        button->setMinimumWidth(72);
        connect(button, &QPushButton::clicked, this, [this, result]() {
            m_result = result;
            accept();
        });
        buttons->addWidget(button);
    };
    addButton(tr("Save"), QMessageBox::Save);
    addButton(tr("Discard"), QMessageBox::Discard);
    addButton(tr("Cancel"), QMessageBox::Cancel);
    bodyLayout->addLayout(buttons);
    root->addWidget(body);
}

QMessageBox::StandardButton UnsavedChangesDialog::result() const
{
    return m_result;
}

void setupFramelessMessageBox(QMessageBox* messageBox)
{
    if (!messageBox || messageBox->property("cakeryCustomTitleBar").toBool()) {
        return;
    }

    auto* grid = qobject_cast<QGridLayout*>(messageBox->layout());
    if (!grid) {
        return;
    }

    struct GridItem {
        QLayoutItem* item = nullptr;
        int row = 0;
        int column = 0;
        int rowSpan = 1;
        int columnSpan = 1;
    };
    DynamicArray<GridItem> items;
    items.reserve(static_cast<Size_t>(grid->count()));
    int columnCount = 1;
    for (int index = 0; index < grid->count(); ++index) {
        int row = 0;
        int column = 0;
        int rowSpan = 1;
        int columnSpan = 1;
        grid->getItemPosition(index, &row, &column, &rowSpan, &columnSpan);
        items.push_back({nullptr, row, column, rowSpan, columnSpan});
        columnCount = qMax(columnCount, column + columnSpan);
    }
    for (int index = static_cast<int>(items.size()) - 1; index >= 0; --index) {
        items[static_cast<Size_t>(index)].item = grid->takeAt(index);
    }

    messageBox->setProperty("cakeryCustomTitleBar", true);
    messageBox->setProperty("editorDialog", true);
    messageBox->setAttribute(Qt::WA_TranslucentBackground, false);
    messageBox->setAttribute(Qt::WA_StyledBackground, true);
    messageBox->setAutoFillBackground(true);
    messageBox->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    grid->addWidget(new FloatingWindowTitleBar(messageBox, false), 0, 0, 1, columnCount);
    for (const auto& entry : items) {
        grid->addItem(entry.item, entry.row + 1, entry.column,
                      entry.rowSpan, entry.columnSpan);
    }
    messageBox->show();
    applyWindowsFrameAttributes(messageBox);
}

QColor selectEditorColor(const QColor& initial, QWidget* parent,
                         const QString& title, bool showAlpha)
{
    QColorDialog dialog(parent);
    dialog.setOption(QColorDialog::DontUseNativeDialog);
    dialog.setOption(QColorDialog::ShowAlphaChannel, showAlpha);
    dialog.setCurrentColor(initial);
    dialog.setWindowTitle(title);
    dialog.setProperty("editorWorkspace", true);
    dialog.setObjectName(QStringLiteral("editorColorDialog"));
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setAttribute(Qt::WA_StyledBackground, true);
    dialog.layout()->setMenuBar(new FloatingWindowTitleBar(&dialog, false));
    return dialog.exec() == QDialog::Accepted ? dialog.selectedColor() : QColor();
}

ads::CFloatingDockContainer* floatingWindowAncestor(QObject* object)
{
    auto* widget = qobject_cast<QWidget*>(object);
    while (widget) {
        if (auto* floating = qobject_cast<ads::CFloatingDockContainer*>(widget)) {
            return floating;
        }
        widget = widget->parentWidget();
    }
    return nullptr;
}

void applyWindowsFrameAttributes(QWidget* widget)
{
#ifdef _WIN32
    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    constexpr DWORD kDwmWindowCornerPreference = 33;
    constexpr DWORD kDwmBorderColor = 34;
    constexpr DWORD kDwmCornerDoNotRound = 1;
    constexpr DWORD kDwmCornerRound = 2;
    constexpr DWORD kDwmColorNone = 0xFFFFFFFE;

    static HMODULE dwmModule = LoadLibraryW(L"dwmapi.dll");
    if (!dwmModule) {
        return;
    }
    const auto setWindowAttribute = reinterpret_cast<DwmSetWindowAttributeFn>(
        GetProcAddress(dwmModule, "DwmSetWindowAttribute"));
    if (!setWindowAttribute) {
        return;
    }

    const bool edgeToEdge = widget->isMaximized() || widget->isFullScreen();
    const DWORD cornerPreference = edgeToEdge ? kDwmCornerDoNotRound : kDwmCornerRound;
    const DWORD borderColor = edgeToEdge ? kDwmColorNone : RGB(21, 21, 21);
    const HWND window = reinterpret_cast<HWND>(widget->winId());
    setWindowAttribute(window, kDwmWindowCornerPreference, &cornerPreference, sizeof(cornerPreference));
    setWindowAttribute(window, kDwmBorderColor, &borderColor, sizeof(borderColor));
#else
    (void)widget;
#endif
}

} // namespace cakery
