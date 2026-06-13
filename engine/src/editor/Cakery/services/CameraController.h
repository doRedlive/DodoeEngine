// do@Redlive

#pragma once

#include <QObject>
#include <algorithm>

namespace dodoe { class Camera; }

namespace cakery {

class CameraController : public QObject {
    Q_OBJECT
public:
    explicit CameraController(dodoe::Camera* engineCamera, QObject* parent = nullptr);

    void setViewportSize(float w, float h);

    void onMouseDown(double x, double y, int button);
    void onMouseUp(int button);
    void onMouseMove(double x, double y);
    void onScroll(double delta);

    float zoom() const { return m_zoom; }
    float positionX() const { return m_posX; }
    float positionY() const { return m_posY; }

    std::pair<float, float> screenToWorld(float screenX, float screenY) const;
    std::pair<float, float> worldToScreen(float worldX, float worldY) const;

private:
    void updateCamera();

    float m_viewportW = 800.0f;
    float m_viewportH = 600.0f;
    float m_zoom = 1.0f;
    float m_posX = 0.0f;
    float m_posY = 0.0f;

    bool m_isDragging = false;
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;

    dodoe::Camera* m_engineCamera = nullptr;

    static constexpr float kMinZoom = 0.01f;
    static constexpr float kMaxZoom = 100.0f;
    static constexpr float kZoomFactor = 1.1f;
};

} // namespace cakery
