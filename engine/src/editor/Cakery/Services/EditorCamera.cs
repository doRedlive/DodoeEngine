// do@Redlive
// 2D editor camera controller — port of editor_camera_controller.cpp
// Middle-drag = pan, scroll = zoom

namespace Cakery.Services;

public class EditorCamera2D
{
    public float Zoom { get; private set; } = 1.0f;
    public float PositionX { get; private set; }
    public float PositionY { get; private set; }

    private float _viewportW = 800, _viewportH = 600;
    private bool _isDragging;
    private double _lastMouseX, _lastMouseY;

    private const float MinZoom = 0.01f;
    private const float MaxZoom = 100f;
    private const float ZoomFactor = 1.1f;

    public void SetViewportSize(float w, float h)
    {
        _viewportW = w;
        _viewportH = h;
    }

    public void OnMouseDown(double x, double y, int button)
    {
        if (button == 1) // middle button
        {
            _isDragging = true;
            _lastMouseX = x;
            _lastMouseY = y;
        }
    }

    public void OnMouseUp(int button)
    {
        if (button == 1) _isDragging = false;
    }

    public void OnMouseMove(double x, double y)
    {
        if (!_isDragging) return;
        float dx = (float)(x - _lastMouseX);
        float dy = (float)(y - _lastMouseY);
        _lastMouseX = x;
        _lastMouseY = y;

        // Inverse pan (drag world)
        float worldDx = -dx / Zoom;
        float worldDy = -dy / Zoom;
        PositionX += worldDx;
        PositionY -= worldDy;
    }

    public void OnScroll(double delta)
    {
        float factor = delta > 0 ? ZoomFactor : 1f / ZoomFactor;
        Zoom = Math.Clamp(Zoom * factor, MinZoom, MaxZoom);
    }

    public (float x, float y) ScreenToWorld(float screenX, float screenY)
    {
        float wx = PositionX + (screenX - _viewportW * 0.5f) / Zoom;
        float wy = PositionY - (screenY - _viewportH * 0.5f) / Zoom;
        return (wx, wy);
    }

    public (float x, float y) WorldToScreen(float worldX, float worldY)
    {
        float sx = (worldX - PositionX) * Zoom + _viewportW * 0.5f;
        float sy = -(worldY - PositionY) * Zoom + _viewportH * 0.5f;
        return (sx, sy);
    }
}
