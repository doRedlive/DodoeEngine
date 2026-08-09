// do@Redlive

#include "GamePanel.h"
#include "framework/EditorContext.h"
#include "framework/viewport/ViewportService.h"
#include "framework/playmode/PlayModeController.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QPainter>

namespace cakery {

GamePanel::GamePanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setMinimumSize(320, 240);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    buildToolbar();

    m_noCameraLabel = new QLabel("No Cameras Rendering", this);
    m_noCameraLabel->setAlignment(Qt::AlignCenter);
    m_noCameraLabel->setStyleSheet("color: #888; font-size: 14px;");
    mainLayout->addWidget(m_noCameraLabel);
}

GamePanel::~GamePanel()
{
    if (m_vp) {
        m_ctx.viewports().unregisterViewport(m_vp);
    }
}

void GamePanel::buildToolbar()
{
    auto* bar = new QHBoxLayout();

    m_aspectCombo = new QComboBox(this);
    m_aspectCombo->addItem("Free Aspect", 0.0f);
    m_aspectCombo->addItem("16:9", 16.0f / 9.0f);
    m_aspectCombo->addItem("9:16", 9.0f / 16.0f);
    m_aspectCombo->addItem("4:3", 4.0f / 3.0f);
    m_aspectCombo->addItem("1:1", 1.0f);
    bar->addWidget(m_aspectCombo);

    connect(m_aspectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GamePanel::onAspectChanged);

    m_maximizeCheck = new QCheckBox("Maximize On Play", this);
    bar->addWidget(m_maximizeCheck);

    bar->addStretch();

    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (layout) {
        layout->insertLayout(0, bar);
    }
}

void GamePanel::onAspectChanged(int index)
{
    float aspect = m_aspectCombo->itemData(index).toFloat();
    if (m_vp) {
        m_ctx.viewports().setGameAspect(m_vp, aspect);
    }
}

void GamePanel::showEvent(QShowEvent* event)
{
    Panel::showEvent(event);
    if (m_firstShow) {
        m_firstShow = false;
        registerViewportIfNeeded();
    }
}

void GamePanel::resizeEvent(QResizeEvent* event)
{
    Panel::resizeEvent(event);
    registerViewportIfNeeded();
    if (m_vp) {
        float dpr = devicePixelRatioF();
        m_ctx.viewports().onResized(m_vp, event->size().width(), event->size().height(), dpr);
    }
}

void GamePanel::registerViewportIfNeeded()
{
    if (m_vp || !m_ctx.isBooted()) return;
    float dpr = devicePixelRatioF();
    m_vp = m_ctx.viewports().registerViewport(
        ViewportKind::Game,
        reinterpret_cast<void*>(winId()),
        width(), height(), dpr);
}

void GamePanel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));
}

} // namespace cakery
