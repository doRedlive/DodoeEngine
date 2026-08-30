// do@Redlive

#include "EditorCamera.h"
#include "runtime/core/channel/camera_channel.h"
#include "runtime/function/render/render_settings.h"

#include <algorithm>
#include <cmath>

namespace cakery {

EditorCamera::EditorCamera() = default;

void EditorCamera::setViewportSize(float w, float h)
{
    m_vpW = w;
    m_vpH = h;
}

void EditorCamera::setMode(Mode mode)
{
    if (mode == m_mode) {
        return;
    }
    if (mode == Mode::Ortho2D) {
        m_orthoPan = Vector2f(m_pivot.x, m_pivot.y);
        const float halfH = m_distance * std::tan(glm::radians(m_fov * 0.5f));
        m_orthoZoom = halfH * 2.0f;
    }
    m_mode = mode;
}

void EditorCamera::update(float dt)
{
    if (m_mode == Mode::Orbit) {
        updateOrbit(dt);
    } else if (m_mode == Mode::Fly) {
        updateFly(dt);
    } else {
        updateOrtho2D(dt);
    }
}

void EditorCamera::updateOrbit(float /*dt*/)
{
    m_pitch = std::clamp(m_pitch, -kPitchLimit, kPitchLimit);

    float pitchRad = glm::radians(m_pitch);
    float yawRad   = glm::radians(m_yaw);

    dodoe::Vector3f dir{
        std::cos(pitchRad) * std::cos(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::sin(yawRad)
    };

    dodoe::Vector3f eye = m_pivot - dir * m_distance;

    (void)eye;
}

void EditorCamera::updateFly(float dt)
{
    float speed = kFlySpeed * dt;
    if (m_keyW) m_flyPos += forward() * speed;
    if (m_keyS) m_flyPos -= forward() * speed;
    if (m_keyA) m_flyPos -= right() * speed;
    if (m_keyD) m_flyPos += right() * speed;
    if (m_keyQ) m_flyPos -= dodoe::Vector3f{0.0f, speed, 0.0f};
    if (m_keyE) m_flyPos += dodoe::Vector3f{0.0f, speed, 0.0f};
}

void EditorCamera::updateOrtho2D(float /*dt*/)
{
}

dodoe::Vector3f EditorCamera::forward() const
{
    float pitchRad = glm::radians(m_flyPitch);
    float yawRad   = glm::radians(m_flyYaw);
    return {
        std::cos(pitchRad) * std::cos(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::sin(yawRad)
    };
}

dodoe::Vector3f EditorCamera::right() const
{
    float yawRad = glm::radians(m_flyYaw);
    return {
        -std::sin(yawRad),
        0.0f,
        std::cos(yawRad)
    };
}

void EditorCamera::commitToRenderChannel()
{
    auto& ch = dodoe::GetEditorCameraChannel().get<dodoe::CameraData>();
    ch.view = view();
    ch.projection = projection();
}

void EditorCamera::onMouseDown(float x, float y, int button, bool alt)
{
    if (button >= 0 && button < 3) {
        m_mouseDown[button] = true;
    }
    m_lastMouseX = x;
    m_lastMouseY = y;
    m_altDown = alt;

    if (m_mode == Mode::Ortho2D) {
        return;
    }

    if (button == 2) {
        m_mode = Mode::Fly;
    } else if (m_altDown && button == 0) {
        m_mode = Mode::Orbit;
    }
}

void EditorCamera::onMouseUp(int button)
{
    if (button >= 0 && button < 3) {
        m_mouseDown[button] = false;
    }

    if (m_mode == Mode::Ortho2D) {
        return;
    }

    if (button == 2) {
        m_mode = Mode::Orbit;
    }
}

void EditorCamera::onMouseMove(float x, float y)
{
    float dx = x - m_lastMouseX;
    float dy = y - m_lastMouseY;
    m_lastMouseX = x;
    m_lastMouseY = y;

    if (m_mode == Mode::Orbit) {
        if (m_mouseDown[0] && m_altDown) {
            m_yaw   -= dx * kOrbitSpeed;
            m_pitch += dy * kOrbitSpeed;
        }
        if (m_mouseDown[1]) {
            float speed = m_distance * kPanSpeed;
            m_pivot -= right() * dx * speed;
            m_pivot += dodoe::Vector3f{0.0f, 1.0f, 0.0f} * dy * speed;
        }
    } else if (m_mode == Mode::Fly) {
        m_flyYaw   -= dx * kOrbitSpeed;
        m_flyPitch += dy * kOrbitSpeed;
        m_flyPitch = std::clamp(m_flyPitch, -kPitchLimit, kPitchLimit);
    } else if (m_mode == Mode::Ortho2D) {
        float panSpeed = m_orthoZoom / m_vpH;
        if (m_mouseDown[0] || m_mouseDown[1]) {
            m_orthoPan.x -= dx * panSpeed;
            m_orthoPan.y += dy * panSpeed;
        }
    }
}

void EditorCamera::updateLastMouse(float x, float y)
{
    m_lastMouseX = x;
    m_lastMouseY = y;
}

void EditorCamera::onScroll(float delta)
{
    if (m_mode == Mode::Ortho2D) {
        m_orthoZoom -= delta * kZoomSpeed * 10.0f;
        m_orthoZoom = std::clamp(m_orthoZoom, kOrthoZoomMin, kOrthoZoomMax);
        return;
    }
    m_distance -= delta * kZoomSpeed;
    m_distance = std::clamp(m_distance, kMinDistance, kMaxDistance);
}

void EditorCamera::onKey(int key, bool down)
{
    switch (key) {
    case 'W': m_keyW = down; break;
    case 'S': m_keyS = down; break;
    case 'A': m_keyA = down; break;
    case 'D': m_keyD = down; break;
    case 'Q': m_keyQ = down; break;
    case 'E': m_keyE = down; break;
    default: break;
    }
}

void EditorCamera::focusOn(const dodoe::Vector3f& target, float radius)
{
    m_pivot    = target;
    m_distance = std::max(radius * 2.0f, kMinDistance);
}

dodoe::Matrix4f EditorCamera::view() const
{
    if (m_mode == Mode::Fly) {
        return glm::lookAt(m_flyPos, m_flyPos + forward(), dodoe::Vector3f{0.0f, 1.0f, 0.0f});
    }

    if (m_mode == Mode::Ortho2D) {
        dodoe::Vector3f eye(m_orthoPan.x, m_orthoPan.y, 10.0f);
        dodoe::Vector3f center(m_orthoPan.x, m_orthoPan.y, 0.0f);
        return glm::lookAt(eye, center, dodoe::Vector3f{0.0f, 1.0f, 0.0f});
    }

    float pitchRad = glm::radians(m_pitch);
    float yawRad   = glm::radians(m_yaw);

    dodoe::Vector3f dir{
        std::cos(pitchRad) * std::cos(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::sin(yawRad)
    };

    dodoe::Vector3f eye = m_pivot - dir * m_distance;
    return glm::lookAt(eye, m_pivot, dodoe::Vector3f{0.0f, 1.0f, 0.0f});
}

dodoe::Matrix4f EditorCamera::projection() const
{
    float aspect = (m_vpH > 0.0f) ? (m_vpW / m_vpH) : 1.0f;

    if (m_mode == Mode::Ortho2D) {
        float halfH = m_orthoZoom * 0.5f;
        float halfW = halfH * aspect;
        auto proj = glm::ortho(-halfW, halfW, -halfH, halfH, -100.0f, 100.0f);
        return dodoe::Math::FlipClipSpaceY(proj);
    }

    auto proj = glm::perspective(glm::radians(m_fov), aspect, 0.1f, 10000.0f);
    return dodoe::Math::FlipClipSpaceY(proj);
}

void EditorCamera::screenToRay(float sx, float sy,
                                dodoe::Vector3f& outOrigin, dodoe::Vector3f& outDir) const
{
    dodoe::Matrix4f vp = projection() * view();
    dodoe::Matrix4f invVP = glm::inverse(vp);

    float ndcX = (2.0f * sx) / m_vpW - 1.0f;
    float ndcY = (2.0f * sy) / m_vpH - 1.0f;

    dodoe::Vector4f nearPoint = invVP * dodoe::Vector4f{ndcX, ndcY, 0.0f, 1.0f};
    dodoe::Vector4f farPoint  = invVP * dodoe::Vector4f{ndcX, ndcY, 1.0f, 1.0f};

    nearPoint /= nearPoint.w;
    farPoint  /= farPoint.w;

    outOrigin = dodoe::Vector3f{nearPoint};
    outDir    = glm::normalize(dodoe::Vector3f{farPoint} - outOrigin);
}

dodoe::Vector2f EditorCamera::projectToScreen(const dodoe::Vector3f& worldPos) const
{
    dodoe::Matrix4f vp = projection() * view();
    dodoe::Vector4f clip = vp * dodoe::Vector4f(worldPos, 1.0f);
    if (std::abs(clip.w) < 1e-6f) {
        return {0.0f, 0.0f};
    }
    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;
    const float sx = (ndcX + 1.0f) * 0.5f * m_vpW;
    const float sy = (ndcY + 1.0f) * 0.5f * m_vpH;
    return {sx, sy};
}

dodoe::Vector3f EditorCamera::forwardDirection() const
{
    if (m_mode == Mode::Fly) {
        return forward();
    }
    if (m_mode == Mode::Ortho2D) {
        return {0.0f, 0.0f, -1.0f};
    }

    const float pitchRad = glm::radians(m_pitch);
    const float yawRad   = glm::radians(m_yaw);
    return {
        std::cos(pitchRad) * std::cos(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::sin(yawRad)
    };
}

} // namespace cakery
