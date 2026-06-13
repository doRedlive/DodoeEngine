// do@Redlive

#include "CameraController.h"
#include "runtime/function/render/framework/camera.h"

namespace cakery {

CameraController::CameraController(dodoe::Camera* engineCamera, QObject* parent)
    : QObject(parent)
    , m_engineCamera(engineCamera)
{
    updateCamera();
}

void CameraController::setViewportSize(float w, float h)
{
    m_viewportW = w;
    m_viewportH = h;
}

void CameraController::updateCamera()
{
    if (!m_engineCamera) return;
    m_engineCamera->setPosition(dodoe::Vector3f(m_posX, m_posY, 0.0f));
    m_engineCamera->setZoom(m_zoom);
}

void CameraController::onMouseDown(double x, double y, int button)
{
    if (button == 1) {
        m_isDragging = true;
        m_lastMouseX = x;
        m_lastMouseY = y;
    }
}

void CameraController::onMouseUp(int button)
{
    if (button == 1)
        m_isDragging = false;
}

void CameraController::onMouseMove(double x, double y)
{
    if (!m_isDragging) return;

    float dx = static_cast<float>(x - m_lastMouseX);
    float dy = static_cast<float>(y - m_lastMouseY);
    m_lastMouseX = x;
    m_lastMouseY = y;

    float worldDx = -dx / m_zoom;
    float worldDy = -dy / m_zoom;
    m_posX += worldDx;
    m_posY -= worldDy;

    updateCamera();
}

void CameraController::onScroll(double delta)
{
    float factor = delta > 0.0f ? kZoomFactor : (1.0f / kZoomFactor);
    m_zoom = std::clamp(m_zoom * factor, kMinZoom, kMaxZoom);

    updateCamera();
}

std::pair<float, float> CameraController::screenToWorld(float screenX, float screenY) const
{
    float wx = m_posX + (screenX - m_viewportW * 0.5f) / m_zoom;
    float wy = m_posY - (screenY - m_viewportH * 0.5f) / m_zoom;
    return {wx, wy};
}

std::pair<float, float> CameraController::worldToScreen(float worldX, float worldY) const
{
    float sx = (worldX - m_posX) * m_zoom + m_viewportW * 0.5f;
    float sy = -(worldY - m_posY) * m_zoom + m_viewportH * 0.5f;
    return {sx, sy};
}

} // namespace cakery
