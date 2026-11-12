// do@Redlive

#include "dopch.h"

#include "ProjectManagerWindow.h"

#include "FramelessChrome.h"
#include "ProjectManagerDialogs.h"
#include "ui/common/EditorIcons.h"

#include <QApplication>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QButtonGroup>
#include <QComboBox>
#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QFontMetrics>
#include <QPushButton>
#include <QRadialGradient>
#include <QScrollArea>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

namespace cakery {

namespace {

constexpr int kPreviewMinHeight = 104;
constexpr int kPreviewMaxHeight = 220;
constexpr int kCardBodyHeight = 88;

struct IsoBlock {
    double cx;
    double cy;
    double w;
};

const QVector<QVector<IsoBlock>>& isoLayouts()
{
    static const QVector<QVector<IsoBlock>> layouts = {
        {{168, 116, 58}, {98, 138, 40}, {238, 140, 32}, {132, 160, 22}},
        {{150, 112, 52}, {214, 132, 42}, {76, 146, 26}},
        {{112, 120, 40}, {190, 116, 44}, {236, 142, 28}, {66, 150, 22}},
        {{150, 138, 46}, {104, 148, 28}, {202, 148, 32}},
        {{160, 104, 68}, {224, 142, 24}, {94, 150, 22}}
    };
    return layouts;
}

QColor pmHsl(int hue, double sat, double light, double alpha = 1.0)
{
    return QColor::fromHsl(hue, int(sat * 2.55), int(light * 2.55), int(alpha * 255.0));
}

int hueFor(const QString& name)
{
    static const int palette[] = {38, 205, 145, 268, 12, 190};
    return palette[qHash(name) % 6];
}

int variantFor(const QString& name)
{
    return int(qHash(name) % 5);
}

QString formatBytes(qint64 bytes)
{
    const double gb = bytes / (1024.0 * 1024.0 * 1024.0);
    if (gb < 1.0) {
        const double mb = bytes / (1024.0 * 1024.0);
        return QStringLiteral("%1 MB").arg(mb, 0, 'f', 1);
    }
    return QStringLiteral("%1 GB").arg(gb, 0, 'f', 1);
}

void repolish(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

QJsonObject buildCameraEntity(qint64 uuid, int cameraType)
{
    QJsonObject entity;
    entity["m_uuid"] = uuid;
    entity["m_name"] = QStringLiteral("Primary Camera");
    QJsonArray components;
    QJsonObject tag;
    tag["m_type_name"] = QStringLiteral("TagComponent");
    tag["m_component"] = QStringLiteral("{\"tag\":\"PrimaryCamera\"}");
    components.append(tag);
    QJsonObject transform;
    transform["m_type_name"] = QStringLiteral("TransformComponent");
    transform["m_component"] =
        QStringLiteral("{\"position\":[0.0,0.0,0.0],\"rotation\":[0.0,0.0,0.0],\"scale\":[1.0,1.0,1.0]}");
    components.append(transform);
    QJsonObject camera;
    camera["m_type_name"] = QStringLiteral("CameraComponent");
    camera["m_component"] = QStringLiteral("{\"type\":%1,\"zoom\":1.0,\"background\":[1.0,1.0,1.0,1.0]}")
        .arg(cameraType);
    components.append(camera);
    entity["m_native_components"] = components;
    entity["m_managed_components"] = QJsonArray();
    return entity;
}

QJsonObject buildPlaceholderEntity(qint64 uuid, const QString& name, double x)
{
    QJsonObject entity;
    entity["m_uuid"] = uuid;
    entity["m_name"] = name;
    QJsonArray components;
    QJsonObject tag;
    tag["m_type_name"] = QStringLiteral("TagComponent");
    tag["m_component"] = QStringLiteral("{\"tag\":\"%1\"}").arg(name);
    components.append(tag);
    QJsonObject transform;
    transform["m_type_name"] = QStringLiteral("TransformComponent");
    transform["m_component"] = QStringLiteral(
        "{\"position\":[%1,0.0,0.0],\"rotation\":[0.0,0.0,0.0],\"scale\":[1.0,1.0,1.0]}").arg(x);
    components.append(transform);
    entity["m_native_components"] = components;
    entity["m_managed_components"] = QJsonArray();
    return entity;
}

QJsonArray sceneEntities(const QString& templateId)
{
    QJsonArray entities;
    if (templateId == QStringLiteral("blank"))
        return entities;
    entities.append(buildCameraEntity(1, templateId == QStringLiteral("2d") ? 1 : 0));
    if (templateId == QStringLiteral("tps") || templateId == QStringLiteral("fps")
        || templateId == QStringLiteral("terrain")) {
        entities.append(buildPlaceholderEntity(2, QStringLiteral("BlockA"), 2.0));
        entities.append(buildPlaceholderEntity(3, QStringLiteral("BlockB"), -2.0));
    } else if (templateId == QStringLiteral("render")) {
        for (int i = 0; i < 4; ++i)
            entities.append(buildPlaceholderEntity(2 + i,
                QStringLiteral("MaterialBall%1").arg(i + 1), -3.0 + i * 2.0));
    } else if (templateId == QStringLiteral("2d")) {
        entities.append(buildPlaceholderEntity(2, QStringLiteral("Ground"), 0.0));
    }
    return entities;
}

QString tagLabel(const ProjectEntry& proj)
{
    if (proj.archived)
        return QCoreApplication::translate("ProjectManagerWindow", "Archived");
    if (proj.tags.contains(QStringLiteral("game")))
        return QCoreApplication::translate("ProjectManagerWindow", "Game");
    if (proj.tags.contains(QStringLiteral("demo")))
        return QCoreApplication::translate("ProjectManagerWindow", "Demo");
    return QCoreApplication::translate("ProjectManagerWindow", "Project");
}

} // namespace

ProjectManagerTitleBar::ProjectManagerTitleBar(QWidget* window)
    : QWidget(window)
    , m_window(window)
{
    setObjectName(QStringLiteral("pmTitleBar"));
    setFixedHeight(56);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(22, 0, 18, 0);
    layout->setSpacing(10);

    auto* logo = new QLabel(QStringLiteral("C"), this);
    logo->setObjectName(QStringLiteral("pmLogoMark"));
    logo->setFixedSize(22, 22);
    logo->setAlignment(Qt::AlignCenter);
    logo->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(logo);

    auto* title = new QLabel(tr("Project Manager"), this);
    title->setObjectName(QStringLiteral("pmTitle"));
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(title);

    auto* small = new QLabel(QStringLiteral("Cakery"), this);
    small->setObjectName(QStringLiteral("pmTitleSmall"));
    small->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(small);

    layout->addStretch();

    auto makeWindowButton = [this, layout](const char* name, const char* icon, const char* tip,
                                           auto callback) {
        auto* button = new QToolButton(this);
        button->setObjectName(QLatin1String(name));
        button->setIcon(editorThemedIcon(QLatin1String(icon)));
        button->setIconSize(QSize(14, 14));
        button->setFixedSize(32, 24);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(tr(tip));
        connect(button, &QToolButton::clicked, this, callback);
        layout->addWidget(button);
        return button;
    };
    makeWindowButton("pmWinButton", "minus.svg", "Minimize", [this]() { m_window->showMinimized(); });
    m_maxButton = makeWindowButton("pmWinButton", "maximize-2.svg", "Maximize",
                                   [this]() { toggleMaximize(); });
    makeWindowButton("pmWinClose", "x.svg", "Close", [this]() { m_window->close(); });
}

void ProjectManagerTitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_window && m_window->windowHandle()) {
        m_window->windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ProjectManagerTitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        toggleMaximize();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void ProjectManagerTitleBar::toggleMaximize()
{
    if (!m_window)
        return;
    const bool maximized = m_window->windowState().testFlag(Qt::WindowMaximized)
        || m_window->isFullScreen();
    if (maximized)
        m_window->showNormal();
    else
        m_window->showMaximized();
    if (m_maxButton) {
        const bool restore = m_window->windowState().testFlag(Qt::WindowMaximized)
            || m_window->isFullScreen();
        m_maxButton->setIcon(editorThemedIcon(restore ? QStringLiteral("minimize-2.svg")
                                                       : QStringLiteral("maximize-2.svg")));
        m_maxButton->setToolTip(restore ? tr("Restore Down") : tr("Maximize"));
        QTimer::singleShot(0, this, [this]() {
            if (!m_window || !m_maxButton)
                return;
            const bool restore = m_window->windowState().testFlag(Qt::WindowMaximized)
                || m_window->isFullScreen();
            m_maxButton->setIcon(editorThemedIcon(restore ? QStringLiteral("minimize-2.svg")
                                                           : QStringLiteral("maximize-2.svg")));
            m_maxButton->setToolTip(restore ? tr("Restore Down") : tr("Maximize"));
        });
    }
}

ProjectCard::ProjectCard(const ProjectEntry& entry, qint64 sizeBytes, int hue, int variant,
                         QWidget* parent)
    : QFrame(parent)
    , m_entry(entry)
    , m_hue(hue)
    , m_variant(variant)
{
    setObjectName(QStringLiteral("pmCard"));
    setAttribute(Qt::WA_StyledBackground);
    setCursor(Qt::PointingHandCursor);
    setMinimumWidth(210);
    setFixedHeight(kPreviewMinHeight + kCardBodyHeight);

    auto* cardLayout = new QVBoxLayout(this);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    m_previewSpacer = new QWidget(this);
    m_previewSpacer->setObjectName(QStringLiteral("pmCardPreview"));
    m_previewSpacer->setAttribute(Qt::WA_TranslucentBackground);
    m_previewSpacer->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_previewSpacer->setFixedHeight(kPreviewMinHeight);
    cardLayout->addWidget(m_previewSpacer);

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("pmCardBody"));
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(12, 9, 12, 11);
    bodyLayout->setSpacing(4);

    auto* nameRow = new QHBoxLayout();
    nameRow->setSpacing(6);
    m_dot = new QLabel(body);
    m_dot->setObjectName(QStringLiteral("pmDot"));
    m_dot->setFixedSize(6, 6);
    m_dot->setProperty("state", entry.isMissing ? QStringLiteral("miss") : QStringLiteral("ok"));
    m_dot->setAttribute(Qt::WA_TransparentForMouseEvents);
    nameRow->addWidget(m_dot);
    m_nameLabel = new QLabel(entry.name, body);
    m_nameLabel->setObjectName(QStringLiteral("pmCardName"));
    m_nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    nameRow->addWidget(m_nameLabel);
    nameRow->addStretch();
    bodyLayout->addLayout(nameRow);

    m_pathLabel = new QLabel(body);
    m_pathLabel->setObjectName(QStringLiteral("pmCardPath"));
    m_pathLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    bodyLayout->addWidget(m_pathLabel);

    m_metaLabel = new QLabel(body);
    m_metaLabel->setObjectName(QStringLiteral("pmCardMeta"));
    m_metaLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    bodyLayout->addWidget(m_metaLabel);

    cardLayout->addWidget(body, 1);

    m_tagPill = new QLabel(this);
    m_tagPill->setObjectName(QStringLiteral("pmCardPill"));
    m_tagPill->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_tools = new QWidget(this);
    m_tools->setObjectName(QStringLiteral("pmCardTools"));
    auto* toolsLayout = new QHBoxLayout(m_tools);
    toolsLayout->setContentsMargins(0, 0, 0, 0);
    toolsLayout->setSpacing(4);
    auto makeTool = [this, toolsLayout](const char* icon, const char* tip, auto callback) {
        auto* button = new QToolButton(m_tools);
        button->setObjectName(QStringLiteral("pmCardTool"));
        button->setIcon(editorThemedIcon(QLatin1String(icon)));
        button->setIconSize(QSize(12, 12));
        button->setFixedSize(26, 26);
        button->setToolTip(tr(tip));
        connect(button, &QToolButton::clicked, this, callback);
        toolsLayout->addWidget(button);
    };
    makeTool("play.svg", "Open project", [this]() { if (onOpen) onOpen(); });
    makeTool("folder-up.svg", "Show in Explorer", [this]() { if (onReveal) onReveal(); });
    makeTool("settings.svg", "Project settings", [this]() { if (onSettings) onSettings(); });
    makeTool("trash.svg", "Remove from list", [this]() { if (onRemove) onRemove(); });
    m_tools->adjustSize();
    m_tools->hide();

    setEntry(entry, sizeBytes);
}

void ProjectCard::setEntry(const ProjectEntry& entry, qint64 sizeBytes)
{
    m_entry = entry;
    m_hue = entry.archived ? 220 : hueFor(entry.name);
    m_variant = variantFor(entry.name);

    m_dot->setProperty("state", entry.isMissing ? QStringLiteral("miss") : QStringLiteral("ok"));
    repolish(m_dot);
    m_nameLabel->setText(entry.name);
    m_pathLabel->setText(QFontMetrics(m_pathLabel->font()).elidedText(
        entry.projectFile, Qt::ElideMiddle, qMax(140, width() - 24)));
    m_metaLabel->setText(QStringLiteral("%1  ·  %2")
        .arg(entry.displayDate(), entry.isMissing ? tr("Missing") : formatBytes(sizeBytes)));
    m_tagPill->setText(tagLabel(entry));
    m_tagPill->adjustSize();
    m_tagPill->setGeometry(9, m_previewSpacer->height() - m_tagPill->height() - 9,
                           m_tagPill->width(), m_tagPill->height());
    update();
}

void ProjectCard::setSelected(bool selected)
{
    if (m_selected == selected)
        return;
    m_selected = selected;
    setProperty("sel", selected);
    repolish(this);
    update();
}

void ProjectCard::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);

    QPainter painter(this);
    const QRect thumbnailRect = rect().adjusted(1, 1, -1, 0);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect().adjusted(1, 1, -1, -1)), 11, 11);
    painter.setClipPath(clip);
    drawThumbnail(painter, QRect(thumbnailRect.left(), thumbnailRect.top(),
                                 thumbnailRect.width(), m_previewSpacer->height() - 1));
}

void ProjectCard::drawThumbnail(QPainter& painter, const QRect& rect) const
{
    painter.save();
    const double sx = rect.width() / 320.0;
    const double sy = rect.height() / 180.0;
    painter.scale(sx, sy);

    const int hue = m_hue;
    const double satScale = m_entry.archived ? 0.25 : 1.0;
    const auto hsl = [hue, satScale](double s, double l, double a = 1.0) {
        return pmHsl(hue, s * satScale, l, a);
    };

    QLinearGradient sky(0, 0, 0, 180);
    sky.setColorAt(0.0, hsl(26, 20));
    sky.setColorAt(0.62, hsl(22, 13));
    sky.setColorAt(1.0, hsl(18, 9));
    painter.fillRect(0, 0, 320, 180, sky);

    static constexpr int glowCx[] = {118, 206, 164, 96, 232, 188};
    const QPointF glowCenter(glowCx[m_variant % 6], 90 + (m_variant % 3) * 5);
    QRadialGradient glow(glowCenter, 110 + (m_variant % 3) * 12);
    glow.setColorAt(0.0, hsl(62, 62, 0.42));
    glow.setColorAt(1.0, hsl(62, 62, 0.0));
    painter.fillRect(0, 0, 320, 180, glow);

    painter.setRenderHint(QPainter::Antialiasing, false);
    QPen gridPen(hsl(45, 68));
    gridPen.setWidthF(1.0 / sx);
    painter.setPen(gridPen);
    for (int i = 1; i <= 7; ++i) {
        const double y = 96.0 + i * i * 2.6;
        if (y > 182)
            break;
        painter.setOpacity(0.26 - i * 0.028);
        painter.drawLine(QLineF(0, y, 320, y));
    }
    painter.setOpacity(0.14);
    for (int k = -6; k <= 6; ++k)
        painter.drawLine(QLineF(160 + k * 46, 180, 160 + k * 9, 96));
    painter.setOpacity(0.22);
    painter.drawLine(QLineF(0, 96, 320, 96));
    painter.setOpacity(1.0);

    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen edgePen(hsl(55, 66, 0.5));
    edgePen.setWidthF(1.0 / sx);
    for (const auto& block : isoLayouts()[m_variant]) {
        const double cx = block.cx, cy = block.cy, w = block.w;
        const double h = w * 0.5, d = w * 0.55;
        painter.setPen(Qt::NoPen);
        painter.setBrush(hsl(24, 21));
        painter.drawPolygon(QPolygonF() << QPointF(cx - w / 2, cy) << QPointF(cx, cy + h / 2)
            << QPointF(cx, cy + h / 2 + d) << QPointF(cx - w / 2, cy + d));
        painter.setBrush(hsl(22, 14));
        painter.drawPolygon(QPolygonF() << QPointF(cx + w / 2, cy) << QPointF(cx, cy + h / 2)
            << QPointF(cx, cy + h / 2 + d) << QPointF(cx + w / 2, cy + d));
        painter.setBrush(hsl(30, 33));
        painter.drawPolygon(QPolygonF() << QPointF(cx, cy - h / 2) << QPointF(cx + w / 2, cy)
            << QPointF(cx, cy + h / 2) << QPointF(cx - w / 2, cy));
        painter.setPen(edgePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(QPolygonF() << QPointF(cx - w / 2, cy) << QPointF(cx, cy + h / 2)
            << QPointF(cx + w / 2, cy));
    }

    QLinearGradient fade(0, 99, 0, 180);
    fade.setColorAt(0.0, QColor(0, 0, 0, 0));
    fade.setColorAt(1.0, QColor(0, 0, 0, 115));
    painter.fillRect(0, 0, 320, 180, fade);
    painter.restore();
}

void ProjectCard::enterEvent(QEnterEvent* event)
{
    QFrame::enterEvent(event);
    m_tools->show();
}

void ProjectCard::leaveEvent(QEvent* event)
{
    QFrame::leaveEvent(event);
    if (!rect().contains(mapFromGlobal(QCursor::pos())))
        m_tools->hide();
}

void ProjectCard::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && onSelect)
        onSelect();
    QFrame::mousePressEvent(event);
}

void ProjectCard::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && onOpen)
        onOpen();
    QFrame::mouseDoubleClickEvent(event);
}

void ProjectCard::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);

    const int preview = qBound(kPreviewMinHeight, width() * 9 / 16, kPreviewMaxHeight);
    if (m_previewSpacer->height() != preview) {
        m_previewSpacer->setFixedHeight(preview);
        setFixedHeight(preview + kCardBodyHeight);
    }
    m_tools->setGeometry(width() - m_tools->width() - 8, 8, m_tools->width(), 26);
    m_tagPill->setGeometry(9, preview - m_tagPill->height() - 9,
                           m_tagPill->width(), m_tagPill->height());
    m_pathLabel->setText(QFontMetrics(m_pathLabel->font()).elidedText(
        m_entry.projectFile, Qt::ElideMiddle, qMax(140, width() - 24)));
}

ProjectManagerWindow::ProjectManagerWindow(ProjectConfig& config, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
{
    setWindowTitle(tr("Cakery - Project Manager"));
    setObjectName("ProjectManagerWindow");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground);
    setAutoFillBackground(true);
    resize(1180, 760);
    setMinimumSize(920, 600);
    qApp->installEventFilter(this);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_shell = new QFrame(this);
    m_shell->setObjectName(QStringLiteral("pmShell"));
    m_shell->setAttribute(Qt::WA_StyledBackground);
    rootLayout->addWidget(m_shell);

    auto* shellLayout = new QVBoxLayout(m_shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);
    shellLayout->addWidget(createTitleBar());

    m_bodyLayout = new QHBoxLayout();
    m_bodyLayout->setContentsMargins(16, 0, 16, 14);
    m_bodyLayout->setSpacing(16);
    m_bodyLayout->addWidget(createSidebar());
    m_bodyLayout->addWidget(createMainArea(), 1);
    shellLayout->addLayout(m_bodyLayout, 1);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &ProjectManagerWindow::onSearchChanged);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProjectManagerWindow::onSortChanged);
    connect(m_filterGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_filter = id == 1 ? QStringLiteral("all") : id == 2 ? QStringLiteral("archive")
                                                             : QStringLiteral("recent");
        m_tagFilter.clear();
        for (auto* button : m_tagGroup->buttons())
            button->setChecked(false);
        updateChrome();
        applyFilterList();
        rebuildGrid();
    });
    connect(m_tagGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_tagFilter = id == 0 ? QStringLiteral("game") : QStringLiteral("demo");
        m_filter = QStringLiteral("all");
        m_filterAllBtn->setChecked(true);
        m_tabAll->setChecked(true);
        updateChrome();
        applyFilterList();
        rebuildGrid();
    });
    connect(m_tabGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_filter = id == 1 ? QStringLiteral("all") : id == 2 ? QStringLiteral("archive")
                                                             : QStringLiteral("recent");
        m_tagFilter.clear();
        for (auto* button : m_tagGroup->buttons())
            button->setChecked(false);
        for (auto* button : m_filterGroup->buttons()) {
            const int filterId = m_filterGroup->id(button);
            button->setChecked(filterId == (id == 1 ? 1 : id == 2 ? 2 : 0));
        }
        updateChrome();
        applyFilterList();
        rebuildGrid();
    });

    m_config.load();
    applyFilterList();
    rebuildGrid();
    updateChrome();
}

ProjectManagerWindow::~ProjectManagerWindow()
{
    qApp->removeEventFilter(this);
}

QWidget* ProjectManagerWindow::createTitleBar()
{
    return new ProjectManagerTitleBar(this);
}

QWidget* ProjectManagerWindow::createSidebar()
{
    auto* sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("pmSidebar"));
    sidebar->setFixedWidth(228);
    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(10, 14, 10, 10);
    layout->setSpacing(4);

    auto makeLabel = [this, layout](const QString& text) {
        auto* label = new QLabel(text, this);
        label->setObjectName(QStringLiteral("pmSideLabel"));
        layout->addWidget(label);
        layout->addSpacing(6);
    };
    auto makeSideButton = [this, layout](const QString& text) {
        auto* button = new QPushButton(text, this);
        button->setObjectName(QStringLiteral("pmSideItem"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        layout->addWidget(button);
        return button;
    };

    makeLabel(tr("Library"));
    m_filterGroup = new QButtonGroup(this);
    m_filterGroup->setExclusive(true);
    m_filterRecentBtn = makeSideButton(tr("Recent Projects"));
    m_filterAllBtn = makeSideButton(tr("All Projects"));
    m_filterArchiveBtn = makeSideButton(tr("Archived"));
    m_filterRecentBtn->setChecked(true);
    m_filterGroup->addButton(m_filterRecentBtn, 0);
    m_filterGroup->addButton(m_filterAllBtn, 1);
    m_filterGroup->addButton(m_filterArchiveBtn, 2);

    layout->addSpacing(14);
    makeLabel(tr("Tags"));
    m_tagGroup = new QButtonGroup(this);
    m_tagGroup->setExclusive(true);
    m_tagGameBtn = makeSideButton(tr("Game Projects"));
    m_tagDemoBtn = makeSideButton(tr("Samples & Tests"));
    m_tagGroup->addButton(m_tagGameBtn, 0);
    m_tagGroup->addButton(m_tagDemoBtn, 1);

    layout->addSpacing(14);
    makeLabel(tr("Other"));
    auto* templatesButton = makeSideButton(tr("Template Library"));
    auto* settingsButton = makeSideButton(tr("Preferences"));
    templatesButton->setCheckable(false);
    settingsButton->setCheckable(false);
    connect(templatesButton, &QPushButton::clicked, this, [this] {
        QMessageBox box(this);
        box.setWindowTitle(tr("Template Library"));
        box.setText(tr("Templates are offered while creating a new project."));
        setupFramelessMessageBox(&box);
        box.exec();
    });
    connect(settingsButton, &QPushButton::clicked, this, [this] {
        QMessageBox box(this);
        box.setWindowTitle(tr("Preferences"));
        box.setText(tr("Preferences will be available in a future build."));
        setupFramelessMessageBox(&box);
        box.exec();
    });

    layout->addStretch();
    return sidebar;
}

QWidget* ProjectManagerWindow::createMainArea()
{
    auto* main = new QWidget(this);
    main->setObjectName(QStringLiteral("pmMain"));
    auto* mainLayout = new QVBoxLayout(main);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* head = new QWidget(main);
    head->setObjectName(QStringLiteral("pmHead"));
    auto* headLayout = new QVBoxLayout(head);
    headLayout->setContentsMargins(6, 8, 4, 0);
    headLayout->setSpacing(0);

    auto* headRow = new QHBoxLayout();
    headRow->setSpacing(14);
    auto* headText = new QVBoxLayout();
    headText->setSpacing(3);
    m_viewTitle = new QLabel(tr("Recent Projects"), head);
    m_viewTitle->setObjectName(QStringLiteral("pmViewTitle"));
    m_viewSub = new QLabel(head);
    m_viewSub->setObjectName(QStringLiteral("pmViewSub"));
    headText->addWidget(m_viewTitle);
    headText->addWidget(m_viewSub);
    headRow->addLayout(headText);
    headRow->addStretch();

    m_searchEdit = new QLineEdit(head);
    m_searchEdit->setPlaceholderText(tr("Search name or path"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(220);
    headRow->addWidget(m_searchEdit);

    m_sortCombo = new QComboBox(head);
    m_sortCombo->addItems({tr("Last Modified"), tr("Name"), tr("Size")});
    m_sortCombo->setMinimumWidth(168);
    m_sortCombo->view()->setMinimumWidth(184);
    headRow->addWidget(m_sortCombo);

    auto* importButton = new QPushButton(editorThemedIcon(QStringLiteral("upload.svg")),
                                         tr("Import Project"), head);
    importButton->setObjectName(QStringLiteral("pmBtn"));
    importButton->setCursor(Qt::PointingHandCursor);
    headRow->addWidget(importButton);

    auto* newButton = new QPushButton(editorThemedIcon(QStringLiteral("plus.svg")),
                                      tr("New Project"), head);
    newButton->setObjectName(QStringLiteral("pmPrimaryButton"));
    newButton->setCursor(Qt::PointingHandCursor);
    headRow->addWidget(newButton);

    headLayout->addLayout(headRow);

    auto* tabsHost = new QWidget(head);
    tabsHost->setObjectName(QStringLiteral("pmTabs"));
    auto* tabsLayout = new QHBoxLayout(tabsHost);
    tabsLayout->setContentsMargins(2, 10, 0, 0);
    tabsLayout->setSpacing(2);
    m_tabGroup = new QButtonGroup(this);
    m_tabGroup->setExclusive(true);
    m_tabRecent = new QPushButton(tr("Recent Projects"), tabsHost);
    m_tabAll = new QPushButton(tr("All"), tabsHost);
    m_tabArchive = new QPushButton(tr("Archived"), tabsHost);
    for (auto* tab : {m_tabRecent, m_tabAll, m_tabArchive}) {
        tab->setObjectName(QStringLiteral("pmTab"));
        tab->setCheckable(true);
        tab->setCursor(Qt::PointingHandCursor);
        tabsLayout->addWidget(tab);
    }
    m_tabRecent->setChecked(true);
    m_tabGroup->addButton(m_tabRecent, 0);
    m_tabGroup->addButton(m_tabAll, 1);
    m_tabGroup->addButton(m_tabArchive, 2);
    tabsLayout->addStretch();
    headLayout->addWidget(tabsHost);

    mainLayout->addWidget(head);

    m_scroll = new QScrollArea(main);
    m_scroll->setObjectName(QStringLiteral("pmScroll"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_gridHost = new QWidget(m_scroll);
    m_gridHost->setObjectName(QStringLiteral("pmGridHost"));
    m_grid = new QGridLayout(m_gridHost);
    m_grid->setContentsMargins(16, 16, 16, 20);
    m_grid->setHorizontalSpacing(16);
    m_grid->setVerticalSpacing(16);

    m_emptyHint = new QLabel(tr("No matching projects. Create one to get started."), m_scroll);
    m_emptyHint->setObjectName(QStringLiteral("pmEmptyHint"));
    m_emptyHint->setAlignment(Qt::AlignCenter);

    m_stack = new QStackedWidget(m_scroll);
    m_stack->setObjectName(QStringLiteral("pmStack"));
    m_stack->addWidget(m_gridHost);
    m_stack->addWidget(m_emptyHint);
    m_scroll->setWidget(m_stack);

    mainLayout->addWidget(m_scroll, 1);

    auto* statusHost = new QWidget(main);
    statusHost->setObjectName(QStringLiteral("pmStatusBar"));
    auto* statusLayout = new QHBoxLayout(statusHost);
    statusLayout->setContentsMargins(10, 4, 10, 8);
    statusLayout->setSpacing(16);
    m_statCount = new QLabel(statusHost);
    m_statSize = new QLabel(statusHost);
    m_statSel = new QLabel(statusHost);
    for (auto* label : {m_statCount, m_statSize, m_statSel})
        label->setObjectName(QStringLiteral("pmStatus"));
    statusLayout->addWidget(m_statCount);
    statusLayout->addWidget(m_statSize);
    statusLayout->addStretch();
    statusLayout->addWidget(m_statSel);
    mainLayout->addWidget(statusHost);

    connect(importButton, &QPushButton::clicked, this, &ProjectManagerWindow::onImportProject);
    connect(newButton, &QPushButton::clicked, this, &ProjectManagerWindow::onNewProject);

    return main;
}

void ProjectManagerWindow::applyFilterList()
{
    m_filtered.clear();
    const QString search = m_searchEdit->text().trimmed();

    for (const auto& proj : m_config.projects()) {
        if (m_filter == QStringLiteral("recent") && proj.archived)
            continue;
        if (m_filter == QStringLiteral("archive") && !proj.archived)
            continue;
        if (!m_tagFilter.isEmpty() && !proj.tags.contains(m_tagFilter))
            continue;
        if (!search.isEmpty() &&
            !proj.name.contains(search, Qt::CaseInsensitive) &&
            !proj.projectFile.contains(search, Qt::CaseInsensitive))
            continue;
        m_filtered.append(proj);
    }

    const int sort = m_sortCombo->currentIndex();
    std::sort(m_filtered.begin(), m_filtered.end(),
        [this, sort](const ProjectEntry& a, const ProjectEntry& b) {
            if (sort == 1) return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
            if (sort == 2) return cachedDirSize(a) > cachedDirSize(b);
            return a.lastModifiedUnix > b.lastModifiedUnix;
        });
}

void ProjectManagerWindow::rebuildGrid()
{
    const bool gridWasVisible = m_gridHost->isVisible();
    if (gridWasVisible)
        m_gridHost->hide();
    setUpdatesEnabled(false);
    while (auto* item = m_grid->takeAt(0)) {
        if (auto* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    m_cards.clear();

    for (int i = 0; i < m_filtered.size(); ++i) {
        const ProjectEntry& proj = m_filtered[i];
        auto* card = new ProjectCard(proj, cachedDirSize(proj),
                                     proj.archived ? 220 : hueFor(proj.name),
                                     variantFor(proj.name), m_gridHost);
        card->setSelected(proj.projectFile == m_selectedFile);
        const QString file = proj.projectFile;
        card->onSelect = [this, file]() { selectProject(file); };
        card->onOpen = [this, file]() {
            selectProject(file);
            openProjectFile(file);
        };
        card->onReveal = [this, file]() {
            const QString dir = QFileInfo(file).absolutePath();
            if (!dir.isEmpty() && QDir(dir).exists())
                QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        };
        card->onSettings = [this, file]() { openSettingsFor(file); };
        card->onRemove = [this, file]() { removeProjectFile(file); };
        m_grid->addWidget(card, i / 3, i % 3);
        m_cards.append(card);
    }
    for (int col = 0; col < 3; ++col)
        m_grid->setColumnStretch(col, 1);
    m_grid->setRowStretch(m_filtered.isEmpty() ? 0 : m_filtered.size() / 3 + 1, 1);
    m_grid->activate();
    m_stack->setCurrentIndex(m_filtered.isEmpty() ? 1 : 0);
    if (gridWasVisible)
        m_gridHost->show();

    updateChrome();
    setUpdatesEnabled(true);
    update();
}

void ProjectManagerWindow::updateChrome()
{
    const auto& projects = m_config.projects();
    int archiveCount = 0;
    for (const auto& proj : projects)
        archiveCount += proj.archived ? 1 : 0;
    const int recentCount = projects.size() - archiveCount;

    m_filterRecentBtn->setText(tr("Recent Projects") + QStringLiteral("   %1").arg(recentCount));
    m_filterAllBtn->setText(tr("All Projects") + QStringLiteral("   %1").arg(projects.size()));
    m_filterArchiveBtn->setText(tr("Archived") + QStringLiteral("   %1").arg(archiveCount));

    QString title = tr("Recent Projects");
    if (m_filter == QStringLiteral("all"))
        title = tr("All Projects");
    else if (m_filter == QStringLiteral("archive"))
        title = tr("Archived");
    if (!m_tagFilter.isEmpty())
        title = m_tagFilter == QStringLiteral("game") ? tr("Game Projects")
                                                       : tr("Samples & Tests");
    m_viewTitle->setText(title);
    m_viewSub->setText(tr("%1 project(s)  ·  sorted by %2")
        .arg(m_filtered.size()).arg(m_sortCombo->currentText()));

    qint64 total = 0;
    for (const auto& proj : m_filtered)
        total += cachedDirSize(proj);
    m_statCount->setText(tr("%1 project(s)").arg(m_filtered.size()));
    m_statSize->setText(formatBytes(total));
    m_statSel->setText(m_selectedFile.isEmpty()
        ? tr("No project selected")
        : tr("Selected · %1").arg(QFileInfo(m_selectedFile).completeBaseName()));
}

void ProjectManagerWindow::selectProject(const QString& projectFile)
{
    m_selectedFile = projectFile;
    for (int i = 0; i < m_cards.size() && i < m_filtered.size(); ++i)
        m_cards[i]->setSelected(m_filtered[i].projectFile == projectFile);
    m_statSel->setText(m_selectedFile.isEmpty()
        ? tr("No project selected")
        : tr("Selected · %1").arg(QFileInfo(m_selectedFile).completeBaseName()));
}

void ProjectManagerWindow::openProjectFile(const QString& projectFile)
{
    for (const auto& proj : m_config.projects()) {
        if (proj.projectFile != projectFile || proj.isMissing)
            continue;
        if (!QFileInfo::exists(projectFile))
            return;
        m_config.markOpened(projectFile);
        m_config.save();
        emit onProjectOpened(projectFile);
        accept();
        return;
    }
}

void ProjectManagerWindow::removeProjectFile(const QString& projectFile)
{
    for (const auto& proj : m_config.projects()) {
        if (proj.projectFile != projectFile)
            continue;
        m_config.removeProject(projectFile);
        m_config.save();
        if (m_selectedFile == projectFile)
            m_selectedFile.clear();
        applyFilterList();
        rebuildGrid();
        QMessageBox box(this);
        box.setWindowTitle(tr("Removed"));
        box.setText(tr("Removed %1 from the library. Files on disk are untouched.")
            .arg(proj.name));
        setupFramelessMessageBox(&box);
        box.exec();
        return;
    }
}

void ProjectManagerWindow::openSettingsFor(const QString& projectFile)
{
    QList<ProjectEntry> entries = m_config.projects();
    int index = -1;
    for (int i = 0; i < entries.size(); ++i) {
        if (entries[i].projectFile == projectFile) {
            index = i;
            break;
        }
    }
    if (index < 0)
        return;

    ProjectSettingsDialog dialog(this, entries[index]);
    dialog.exec();
    m_config.replaceProject(index, entries[index]);
    m_config.save();
    applyFilterList();
    rebuildGrid();
    selectProject(projectFile);
}

qint64 ProjectManagerWindow::cachedDirSize(const ProjectEntry& proj)
{
    auto it = m_sizeCache.find(proj.projectFile);
    if (it != m_sizeCache.end())
        return *it;

    qint64 total = 0;
    if (!proj.isMissing && !proj.projectPath.isEmpty()) {
        QDirIterator dirIt(proj.projectPath, QDir::Files, QDirIterator::Subdirectories);
        while (dirIt.hasNext()) {
            dirIt.next();
            total += dirIt.fileInfo().size();
        }
    }
    m_sizeCache.insert(proj.projectFile, total);
    return total;
}

void ProjectManagerWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange && m_bodyLayout && m_shell) {
        const bool edgeToEdge = isMaximized() || isFullScreen();
        m_bodyLayout->setContentsMargins(edgeToEdge ? 0 : 16, 0,
                                         edgeToEdge ? 0 : 16, edgeToEdge ? 0 : 14);
        m_shell->setProperty("edgeToEdge", edgeToEdge);
        repolish(m_shell);
        applyWindowsFrameAttributes(this);
    }
    QDialog::changeEvent(event);
}

bool ProjectManagerWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == this && event->type() == QEvent::Show) {
        applyWindowsFrameAttributes(this);
    }
    if (event->type() == QEvent::Show) {
        if (auto* box = qobject_cast<QMessageBox*>(watched)) {
            if (box->parentWidget() && box->parentWidget()->window() == this)
                setupFramelessMessageBox(box);
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto* widget = qobject_cast<QWidget*>(watched);
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (widget && mouse->button() == Qt::LeftButton) {
            auto belongsToManager = [this](QWidget* candidate) {
                for (QWidget* window = candidate ? candidate->window() : nullptr;
                     window; window = window->parentWidget()) {
                    if (window == this)
                        return true;
                }
                return false;
            };
            if (belongsToManager(widget)) {
                QWidget* focus = focusWidget();
                QWidget* target = QApplication::widgetAt(mouse->globalPosition().toPoint());
                bool clickedInsideFocus = false;
                bool clickedFocusable = false;
                for (QWidget* ancestor = target; ancestor; ancestor = ancestor->parentWidget()) {
                    if (ancestor == focus)
                        clickedInsideFocus = true;
                    if (ancestor->focusPolicy() != Qt::NoFocus
                        || qobject_cast<QAbstractButton*>(ancestor)
                        || qobject_cast<QLineEdit*>(ancestor)
                        || qobject_cast<QComboBox*>(ancestor)) {
                        clickedFocusable = true;
                        break;
                    }
                }
                if (focus && !clickedInsideFocus && !clickedFocusable)
                    focus->clearFocus();
            }
        }
        if (widget && widget->window() == this && windowHandle()
            && !isMaximized() && !isFullScreen()) {
            if (mouse->button() == Qt::LeftButton) {
                bool pressedOnCard = false;
                for (const QObject* ancestor = widget; ancestor; ancestor = ancestor->parent()) {
                    if (const auto* ancestorWidget = qobject_cast<const QWidget*>(ancestor)) {
                        if (ancestorWidget->objectName() == QStringLiteral("pmCard")) {
                            pressedOnCard = true;
                            break;
                        }
                    }
                }
                if (!pressedOnCard && !m_selectedFile.isEmpty())
                    selectProject(QString());

                const QPoint pos = mapFromGlobal(mouse->globalPosition().toPoint());
                constexpr int border = 6;
                Qt::Edges edges;
                if (pos.x() <= border) edges |= Qt::LeftEdge;
                if (pos.x() >= width() - border) edges |= Qt::RightEdge;
                if (pos.y() <= border) edges |= Qt::TopEdge;
                if (pos.y() >= height() - border) edges |= Qt::BottomEdge;
                if (edges != Qt::Edges() && windowHandle()->startSystemResize(edges))
                    return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void ProjectManagerWindow::onOpenProject()
{
    if (!m_selectedFile.isEmpty())
        openProjectFile(m_selectedFile);
}

void ProjectManagerWindow::onNewProject()
{
    QStringList existingNames;
    for (const auto& proj : m_config.projects())
        existingNames.append(proj.name);

    const QString documents = QDir(QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("CakeryProjects"));
    QDir().mkpath(documents);

    NewProjectDialog dialog(this, existingNames, documents + QDir::separator());
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString name = dialog.projectName();
    const QString fullDir = dialog.projectPath();
    if (QDir(fullDir).exists()) {
        QMessageBox box(this);
        box.setWindowTitle(tr("Create Project"));
        box.setText(tr("A project directory with this name already exists."));
        setupFramelessMessageBox(&box);
        box.exec();
        return;
    }

    QDir().mkpath(QDir(fullDir).filePath(QStringLiteral("Assets/Scenes")));
    QDir().mkpath(QDir(fullDir).filePath(QStringLiteral("Binaries")));
    QDir().mkpath(QDir(fullDir).filePath(QStringLiteral("Configs")));

    const QString projFile = QDir(fullDir).filePath(name + QStringLiteral(".doproj"));
    QJsonObject projObj;
    projObj["Name"] = name;
    projObj["Version"] = "1.0";
    projObj["ProjectPath"] = name + QStringLiteral(".doproj");
    projObj["AssetDirectory"] = QStringLiteral("Assets");
    projObj["StartSceneName"] = QStringLiteral("Main");
    projObj["Platform"] = QStringLiteral("windows");
    projObj["BuildConfig"] = QStringLiteral("dev");
    projObj["RestoreLastScene"] = true;
    projObj["HotReload"] = true;
    projObj["SandboxMode"] = false;

    QJsonObject root;
    root["Project"] = projObj;

    QFile file(projFile);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox box(this);
        box.setWindowTitle(tr("Create Project"));
        box.setText(tr("Could not create the project file."));
        setupFramelessMessageBox(&box);
        box.exec();
        QDir(fullDir).removeRecursively();
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    QJsonObject sceneRoot;
    sceneRoot["m_name"] = QStringLiteral("Main");
    sceneRoot["m_entities"] = sceneEntities(dialog.templateId());
    QFile sceneFile(QDir(fullDir).filePath(QStringLiteral("Assets/Scenes/Main.doscn")));
    if (!sceneFile.open(QIODevice::WriteOnly)) {
        QMessageBox box(this);
        box.setWindowTitle(tr("Create Project"));
        box.setText(tr("Could not create the start scene."));
        setupFramelessMessageBox(&box);
        box.exec();
        QDir(fullDir).removeRecursively();
        return;
    }
    sceneFile.write(QJsonDocument(sceneRoot).toJson(QJsonDocument::Indented));
    sceneFile.close();

    ProjectEntry entry;
    entry.name = name;
    entry.projectFile = projFile;
    entry.lastOpenedUnix = QDateTime::currentSecsSinceEpoch();
    entry.lastModifiedUnix = QDateTime::currentSecsSinceEpoch();
    entry.projectPath = fullDir;
    entry.isMissing = false;
    entry.tags = QStringList{QStringLiteral("game")};

    m_sizeCache.clear();
    m_config.addProject(entry);
    m_config.markOpened(projFile);
    m_config.save();

    emit onProjectOpened(projFile);
    accept();
}

void ProjectManagerWindow::onImportProject()
{
    const QString filePath = QFileDialog::getOpenFileName(this, tr("Import .doproj File"),
        QDir::homePath(), tr("Dodoe Project (*.doproj)"));
    if (filePath.isEmpty())
        return;

    const QFileInfo fi(filePath);
    ProjectEntry entry;
    entry.name = fi.completeBaseName();
    entry.projectFile = fi.absoluteFilePath();
    entry.lastModifiedUnix = fi.lastModified().toSecsSinceEpoch();
    entry.projectPath = fi.absolutePath();
    entry.isMissing = false;

    m_sizeCache.clear();
    m_config.addProject(entry);
    m_config.save();
    applyFilterList();
    rebuildGrid();
    selectProject(entry.projectFile);
}

void ProjectManagerWindow::onScanFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Scan Folder for Projects"));
    if (folder.isEmpty())
        return;

    QDirIterator it(folder, {QStringLiteral("*.doproj")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi(it.filePath());
        ProjectEntry entry;
        entry.name = fi.completeBaseName();
        entry.projectFile = fi.absoluteFilePath();
        entry.lastModifiedUnix = fi.lastModified().toSecsSinceEpoch();
        entry.projectPath = fi.absolutePath();
        entry.isMissing = false;
        m_config.addProject(entry);
    }

    m_sizeCache.clear();
    m_config.save();
    applyFilterList();
    rebuildGrid();
}

void ProjectManagerWindow::onRefresh()
{
    m_config.load();
    m_sizeCache.clear();
    m_selectedFile.clear();
    applyFilterList();
    rebuildGrid();
}

void ProjectManagerWindow::onShowInExplorer()
{
    if (m_selectedFile.isEmpty())
        return;
    const QString dir = QFileInfo(m_selectedFile).absolutePath();
    if (!dir.isEmpty() && QDir(dir).exists())
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void ProjectManagerWindow::onSearchChanged(const QString&)
{
    applyFilterList();
    rebuildGrid();
}

void ProjectManagerWindow::onSortChanged(int)
{
    applyFilterList();
    rebuildGrid();
}

} // namespace cakery
