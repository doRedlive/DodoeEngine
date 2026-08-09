namespace OnlyOne;

using GreenCake;
using System;

public class CameraFollow : CakeBehaviour
{
    public float zoom = 2.5f;
    public float lerpSpeed = 6.0f;

    private float _baseZ;

    public override void Awake()
    {
        var cam = GetComponent<CameraComponent>();
        if (cam != null) cam.Zoom = zoom;
        _baseZ = Transform.Position.z;
    }

    public override void Update()
    {
        var player = PlayerController.Instance;
        if (player == null) return;

        Vector3f cp = Transform.Position;
        Vector3f pp = player.Transform.Position;
        float tx = pp.x;
        float ty = pp.y - GameConst.BodyOffsetY;
        float k = Math.Min(1.0f, lerpSpeed * Time.DeltaTime);
        cp.x += (tx - cp.x) * k;
        cp.y += (ty - cp.y) * k;
        cp.z = _baseZ;
        Transform.Position = cp;
    }
}
