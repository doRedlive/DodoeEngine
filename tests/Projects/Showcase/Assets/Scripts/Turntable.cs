namespace Showcase;

using GreenCake;
using System;

public class Turntable : CakeBehaviour
{
    public float degreesPerSecond = 12.0f;

    public override void Update()
    {
        float dt = Time.DeltaTime;
        if (dt <= 0.0f) return;

        Vector3f r = Transform.Rotation;
        r.y += degreesPerSecond * dt;
        if (r.y >= 360.0f) r.y -= 360.0f;
        Transform.Rotation = r;
    }
}
