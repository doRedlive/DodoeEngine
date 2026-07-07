#include "SceneWidget.h"
#include "services/EngineManager.h"
#include "services/CameraController.h"

#include "runtime/service/world/scene_importer.h"

#include <QShowEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

#ifdef _WIN32
#include <windows.h>
#endif

namespace cakery {

SceneWidget::SceneWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);
    setMinimumSize(320, 240);
}

SceneWidget::~SceneWidget() = default;

void SceneWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_firstShow) {
        m_firstShow = false;
        setupCamera();
    }
}

void SceneWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    auto& engine = EngineManager::getInstance();
    if (engine.isInitialized()) {
        engine.tick();
    }
    update();
}

void SceneWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    auto& engine = EngineManager::getInstance();
    if (engine.isInitialized()) {
        float dpr = devicePixelRatioF();
        engine.resizeViewport(event->size().width(),
                              event->size().height(),
                              dpr);
    }
}

void SceneWidget::setupCamera()
{
    m_camera = new CameraController(this);

    float dpr = devicePixelRatioF();
    m_camera->setViewportSize(static_cast<float>(width() * dpr),
                              static_cast<float>(height() * dpr));
}

void SceneWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_camera) return;
    int button = 0;
    if (event->button() == Qt::MiddleButton) button = 1;
    else if (event->button() == Qt::RightButton) button = 2;
    m_camera->onMouseDown(event->position().x(), event->position().y(), button);
}

void SceneWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_camera) return;
    int button = 0;
    if (event->button() == Qt::MiddleButton) button = 1;
    m_camera->onMouseUp(button);
}

void SceneWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_camera) return;
    m_camera->onMouseMove(event->position().x(), event->position().y());
}

void SceneWidget::wheelEvent(QWheelEvent* event)
{
    if (!m_camera) return;
    m_camera->onScroll(event->angleDelta().y());
}

void SceneWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-cakery-asset") ||
        event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void SceneWidget::dragMoveEvent(QDragMoveEvent* event)
{
    event->acceptProposedAction();
}

void SceneWidget::dropEvent(QDropEvent* event)
{
    auto paths = extractAssetPaths(event->mimeData());
    for (const auto& path : paths) {
        createEntityFromAsset(path);
    }
    event->acceptProposedAction();
}

QStringList SceneWidget::extractAssetPaths(const QMimeData* mime) const
{
    QStringList paths;

    if (mime->hasFormat("application/x-cakery-asset")) {
        QString text = QString::fromUtf8(mime->data("application/x-cakery-asset"));
        for (const auto& line : text.split('\n', Qt::SkipEmptyParts)) {
            paths << line.trimmed();
        }
    }

    if (paths.isEmpty() && mime->hasUrls()) {
        for (const auto& url : mime->urls()) {
            if (url.isLocalFile()) {
                paths << url.toLocalFile();
            }
        }
    }

    return paths;
}

void SceneWidget::createEntityFromAsset(const QString& filePath)
{
    dodoe::SceneImporter::ImportAsset(filePath.toStdString());
}

} // namespace cakery
