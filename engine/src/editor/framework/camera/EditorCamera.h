// do@Redlive

#pragma once

#include "runtime/core/math/math.h"

namespace cakery {

class EditorCamera {
public:
    enum class Mode { Orbit, Fly };

    EditorCamera();

    void setViewportSize(float w, float h);
    void update(float dt);
    void commitToRenderChannel();

    void onMouseDown(float x, float y, int button, bool alt);
    void onMouseUp(int button);
    void onMouseMove(float x, float y);
    void onScroll(float delta);
    void onKey(int key, bool down);

    void focusOn(const dodoe::Vector3f& target, float radius);

    dodoe::Matrix4f view() const;
    dodoe::Matrix4f projection() const;

    void screenToRay(float sx, float sy,
                     dodoe::Vector3f& outOrigin, dodoe::Vector3f& outDir) const;

    dodoe::Vector3f pivot() const { return m_pivot; }
    float distance() const { return m_distance; }

private:
    void updateOrbit(float dt);
    void updateFly(float dt);

    dodoe::Vector3f forward() const;
    dodoe::Vector3f right() const;

    Mode m_mode = Mode::Orbit;

    dodoe::Vector3f m_pivot{0.0f, 0.0f, 0.0f};
    float m_distance = 10.0f;
    float m_yaw   = 0.0f;
    float m_pitch = 0.0f;
    float m_fov   = 60.0f;

    float m_vpW = 1280.0f;
    float m_vpH = 720.0f;

    bool m_mouseDown[3] = {false, false, false};
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;
    bool m_altDown = false;

    dodoe::Vector3f m_flyPos{0.0f, 5.0f, 10.0f};
    float m_flyYaw   = -90.0f;
    float m_flyPitch = 0.0f;
    bool m_keyW = false, m_keyS = false, m_keyA = false, m_keyD = false;
    bool m_keyQ = false, m_keyE = false;

    static constexpr float kMinDistance = 0.1f;
    static constexpr float kMaxDistance = 1000.0f;
    static constexpr float kOrbitSpeed  = 0.3f;
    static constexpr float kPanSpeed    = 0.01f;
    static constexpr float kZoomSpeed   = 0.5f;
    static constexpr float kFlySpeed    = 10.0f;
    static constexpr float kPitchLimit  = 89.0f;
};

} // namespace cakery
