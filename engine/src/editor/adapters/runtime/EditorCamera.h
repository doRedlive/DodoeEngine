// do@Redlive

#pragma once

#include "runtime/core/math/math.h"

namespace cakery {

class EditorCamera {
public:
    enum class Mode { Orbit, Fly, Ortho2D };

    EditorCamera();

    void setViewportSize(float w, float h);
    void setMode(Mode mode);
    [[nodiscard]] Mode mode() const { return m_mode; }
    void update(float dt);
    void commitToRenderChannel();

    void onMouseDown(float x, float y, int button, bool alt);
    void onMouseUp(int button);
    void onMouseMove(float x, float y);
    void onScroll(float delta);
    void onKey(int key, bool down);
    void updateLastMouse(float x, float y);

    void focusOn(const dodoe::Vector3f& target, float radius);

    dodoe::Matrix4f view() const;
    dodoe::Matrix4f projection() const;

    void screenToRay(float sx, float sy,
                     dodoe::Vector3f& outOrigin, dodoe::Vector3f& outDir) const;

    dodoe::Vector2f projectToScreen(const dodoe::Vector3f& worldPos) const;
    dodoe::Vector3f forwardDirection() const;

    dodoe::Vector3f pivot() const { return m_pivot; }
    float distance() const { return m_distance; }

private:
    void updateOrtho2D(float dt);

    dodoe::Vector3f forward() const;
    dodoe::Vector3f right() const;
    dodoe::Vector3f up() const;
    [[nodiscard]] bool looking() const { return m_mouseDown[2]; }

    Mode m_mode = Mode::Orbit;

    dodoe::Vector3f m_position{0.0f, 5.0f, 10.0f};
    dodoe::Vector3f m_pivot{0.0f, 0.0f, 0.0f};
    float m_yaw   = -90.0f;
    float m_pitch = -20.0f;
    float m_fov   = 60.0f;

    float m_distance = 15.0f;
    float m_speed    = 10.0f;

    float m_vpW = 1280.0f;
    float m_vpH = 720.0f;

    bool m_mouseDown[3] = {false, false, false};
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;
    bool m_altDown = false;

    bool m_keyW = false, m_keyS = false, m_keyA = false, m_keyD = false;
    bool m_keyQ = false, m_keyE = false;

    dodoe::Vector2f m_orthoPan{0.0f, 0.0f};
    float m_orthoZoom = 100.0f;

    static constexpr float kPitchLimit   = 89.0f;
    static constexpr float kMinSpeed     = 1.0f;
    static constexpr float kMaxSpeed     = 2000.0f;
    static constexpr float kPanScale     = 0.0015f;
    static constexpr float kZoomScale    = 0.1f;
    static constexpr float kMinDistance  = 0.5f;
    static constexpr float kOrbitSpeed   = 0.15f;
    static constexpr float kLookSpeed    = 0.15f;
    static constexpr float kOrthoZoomSpeed = 5.0f;
    static constexpr float kOrthoZoomMin = 1.0f;
    static constexpr float kOrthoZoomMax = 10000.0f;
};

} // namespace cakery
