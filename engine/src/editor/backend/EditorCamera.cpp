// do@Redlive

#include "dopch.h"

#include "EditorCamera.h"
#include "runtime/core/channel/camera_channel.h"
#include "runtime/function/render/render_settings.h"

#include <algorithm>
#include <cmath>

using namespace dodoe;

namespace cakery {

EditorCamera::EditorCamera()
{
    m_pivot = m_position + forward() * m_distance;
}

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
    } else if (m_mode == Mode::Ortho2D) {
        m_position = m_pivot - forward() * m_distance;
    }
    m_mode = mode;
}

void EditorCamera::update(float dt)
{
    if (m_mode == Mode::Ortho2D) {
        updateOrtho2D(dt);
        return;
    }

    if (!looking()) {
        return;
    }

    const float moveX = (m_keyD ? 1.0f : 0.0f) - (m_keyA ? 1.0f : 0.0f);
    const float moveY = (m_keyW ? 1.0f : 0.0f) - (m_keyS ? 1.0f : 0.0f);
    const float speed = m_speed * (m_fastMove ? 3.0f : 1.0f);
    if (moveX != 0.0f || moveY != 0.0f) {
        m_position += forward() * (moveY * speed * dt);
        m_position += right() * (moveX * speed * dt);
    }
    if (m_keyE) {
        m_position.y += speed * dt;
    }
    if (m_keyQ) {
        m_position.y -= speed * dt;
    }
    m_pivot = m_position + forward() * m_distance;
}

void EditorCamera::updateOrtho2D(float /*dt*/)
{
}

Vector3f EditorCamera::forward() const
{
    const float pitch_rad = glm::radians(m_pitch);
    const float yaw_rad   = glm::radians(m_yaw);
    return {
        std::cos(pitch_rad) * std::cos(yaw_rad),
        std::sin(pitch_rad),
        std::cos(pitch_rad) * std::sin(yaw_rad)
    };
}

Vector3f EditorCamera::right() const
{
    const float yaw_rad = glm::radians(m_yaw);
    return {
        -std::sin(yaw_rad),
        0.0f,
        std::cos(yaw_rad)
    };
}

Vector3f EditorCamera::up() const
{
    const float pitch_rad = glm::radians(m_pitch);
    const float yaw_rad   = glm::radians(m_yaw);
    return {
        -std::cos(yaw_rad) * std::sin(pitch_rad),
        std::cos(pitch_rad),
        -std::sin(yaw_rad) * std::sin(pitch_rad)
    };
}

void EditorCamera::commitToRenderChannel()
{
    auto& ch = GetEditorCameraChannel().get<CameraData>();
    ch.view = view();
    ch.projection = projection();
    ch.background = Vector4f(40.0f / 255.0f, 42.0f / 255.0f, 54.0f / 255.0f, 1.0f);
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

    if (m_mode == Mode::Ortho2D) {
        if (m_mouseDown[2] && m_altDown) {
            m_orthoZoom = std::clamp(m_orthoZoom * std::exp(dy * 0.01f), kOrthoZoomMin, kOrthoZoomMax);
            return;
        }
        float panSpeed = m_orthoZoom / std::max(1.0f, m_vpH);
        if (m_mouseDown[1] || (m_mouseDown[0] && m_altDown)) {
            m_orthoPan.x -= dx * panSpeed;
            m_orthoPan.y += dy * panSpeed;
        }
        return;
    }

    if (m_mouseDown[2] && m_altDown) {
        m_distance = std::max(kMinDistance, m_distance * std::exp(dy * 0.01f));
        m_position = m_pivot - forward() * m_distance;
    } else if (m_mouseDown[0] && m_altDown) {
        m_yaw   += dx * kOrbitSpeed;
        m_pitch -= dy * kOrbitSpeed;
        m_pitch  = std::clamp(m_pitch, -kPitchLimit, kPitchLimit);
        m_position = m_pivot - forward() * m_distance;
    } else if (m_mouseDown[2]) {
        m_yaw   += dx * kLookSpeed;
        m_pitch -= dy * kLookSpeed;
        m_pitch  = std::clamp(m_pitch, -kPitchLimit, kPitchLimit);
    } else if (m_mouseDown[1]) {
        const Vector3f offset =
            (-right() * dx + up() * dy) * (m_distance * kPanScale);
        m_position += offset;
        m_pivot += offset;
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
        m_orthoZoom -= delta * kOrthoZoomSpeed;
        m_orthoZoom = std::clamp(m_orthoZoom, kOrthoZoomMin, kOrthoZoomMax);
        return;
    }

    if (looking()) {
        m_speed = std::clamp(m_speed + delta * 8.0f, kMinSpeed, kMaxSpeed);
        return;
    }

    const float amount = delta * m_distance * kZoomScale;
    m_position += forward() * amount;
    m_distance = std::max(m_distance - amount, kMinDistance);
    m_pivot = m_position + forward() * m_distance;
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
    case 0x01000020: m_fastMove = down; break; // Qt's Shift key, keeping Qt out of the camera backend.
    default: break;
    }
}

void EditorCamera::focusOn(const Vector3f& target, float radius)
{
    if (m_mode == Mode::Ortho2D) {
        m_pivot = target;
        m_orthoPan = Vector2f(target.x, target.y);
        const float aspect = m_vpW / std::max(1.0f, m_vpH);
        m_orthoZoom = std::clamp(radius * 4.0f / std::min(1.0f, aspect),
            kOrthoZoomMin, kOrthoZoomMax);
        return;
    }
    m_pivot    = target;
    m_distance = std::max(radius * 2.0f, kMinDistance);
    m_position = m_pivot - forward() * m_distance;
}

Matrix4f EditorCamera::view() const
{
    if (m_mode == Mode::Ortho2D) {
        Vector3f eye(m_orthoPan.x, m_orthoPan.y, 10.0f);
        Vector3f center(m_orthoPan.x, m_orthoPan.y, 0.0f);
        return glm::lookAt(eye, center, Vector3f{0.0f, 1.0f, 0.0f});
    }

    return glm::lookAt(m_position, m_position + forward(), Vector3f{0.0f, 1.0f, 0.0f});
}

Matrix4f EditorCamera::projection() const
{
    float aspect = (m_vpH > 0.0f) ? (m_vpW / m_vpH) : 1.0f;

    if (m_mode == Mode::Ortho2D) {
        float halfH = m_orthoZoom * 0.5f;
        float halfW = halfH * aspect;
        // The RHI uses top-left viewports on both APIs (negative Vulkan height).
        // Keep one Y-up, zero-to-one clip convention for drawing and picking.
        return glm::orthoRH_ZO(-halfW, halfW, -halfH, halfH, -100.0f, 100.0f);
    }

    return glm::perspectiveRH_ZO(glm::radians(m_fov), aspect, 0.1f, 10000.0f);
}

void EditorCamera::screenToRay(float sx, float sy,
                                Vector3f& outOrigin, Vector3f& outDir) const
{
    Matrix4f vp = projection() * view();
    Matrix4f invVP = glm::inverse(vp);

    float ndcX = (2.0f * sx) / m_vpW - 1.0f;
    float ndcY = 1.0f - (2.0f * sy) / m_vpH;

    Vector4f nearPoint = invVP * Vector4f{ndcX, ndcY, 0.0f, 1.0f};
    Vector4f farPoint  = invVP * Vector4f{ndcX, ndcY, 1.0f, 1.0f};

    nearPoint /= nearPoint.w;
    farPoint  /= farPoint.w;

    outOrigin = Vector3f{nearPoint};
    outDir    = glm::normalize(Vector3f{farPoint} - outOrigin);
}

Vector2f EditorCamera::projectToScreen(const Vector3f& worldPos) const
{
    Matrix4f vp = projection() * view();
    Vector4f clip = vp * Vector4f(worldPos, 1.0f);
    if (std::abs(clip.w) < 1e-6f) {
        return {0.0f, 0.0f};
    }
    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;
    const float sx = (ndcX + 1.0f) * 0.5f * m_vpW;
    const float sy = (1.0f - ndcY) * 0.5f * m_vpH;
    return {sx, sy};
}

Vector3f EditorCamera::forwardDirection() const
{
    if (m_mode == Mode::Ortho2D) {
        return {0.0f, 0.0f, -1.0f};
    }
    return forward();
}

float EditorCamera::pixelsToWorld(const Vector3f& worldPos, float pixelSize) const
{
    float worldPerPx = 1.0f;
    if (m_mode == Mode::Ortho2D) {
        worldPerPx = (m_vpH > 0.0f) ? (m_orthoZoom / m_vpH) : 1.0f;
    } else {
        const float dist = std::max(glm::length(worldPos - m_position), 0.1f);
        worldPerPx = (m_vpH > 0.0f)
            ? (2.0f * dist * std::tan(glm::radians(m_fov * 0.5f)) / m_vpH)
            : 1.0f;
    }
    return worldPerPx * pixelSize;
}

} // namespace cakery
