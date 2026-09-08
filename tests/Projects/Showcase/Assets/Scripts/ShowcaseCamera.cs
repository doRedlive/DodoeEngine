namespace Showcase;

using GreenCake;
using System;

public class ShowcaseCamera : CakeBehaviour
{
    public float radius = 250.0f;
    public float height = 70.0f;
    public float lookAtHeight = 12.0f;
    public float orbitSpeed = 6.0f;
    public float mouseSensitivity = 0.25f;
    public float heightSensitivity = 0.3f;
    public float wheelZoomSpeed = 15.0f;
    public float minRadius = 80.0f;
    public float maxRadius = 480.0f;
    public float minHeight = 12.0f;
    public float maxHeight = 320.0f;

    private float _yaw;
    private float _pitch;

    public override void Awake()
    {
        _yaw = Transform.Rotation.y;
    }

    public override void Update()
    {
        float dt = Time.DeltaTime;
        if (dt <= 0.0f) return;

        _yaw += orbitSpeed * dt;

        Vector2f delta = Input.GetMouseDelta();
        if (delta.x != 0.0f || delta.y != 0.0f)
        {
            _yaw += delta.x * mouseSensitivity;
            height = Clamp(height + delta.y * heightSensitivity, minHeight, maxHeight);
        }

        Vector2f wheel = Input.GetMouseWheel();
        if (wheel.y != 0.0f)
        {
            radius = Clamp(radius - wheel.y * wheelZoomSpeed, minRadius, maxRadius);
        }

        if (_yaw >= 360.0f) _yaw -= 360.0f;
        if (_yaw < 0.0f) _yaw += 360.0f;

        float rad = (float)Math.PI / 180.0f;
        float yawRad = _yaw * rad;
        float x = (float)Math.Sin(yawRad) * radius;
        float z = (float)Math.Cos(yawRad) * radius;

        _pitch = -(float)Math.Atan2(height - lookAtHeight, radius) / rad;

        Transform.Position = new Vector3f(x, height, z);
        Transform.Rotation = new Vector3f(_pitch, _yaw, 0.0f);
    }

    private static float Clamp(float v, float lo, float hi)
    {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }
}
