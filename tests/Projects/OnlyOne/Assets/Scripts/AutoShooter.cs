namespace OnlyOne;

using GreenCake;
using System;

public class AutoShooter : CakeBehaviour
{
    public float fireInterval = 0.5f;
    public float damage = 1.0f;

    private float _timer;

    public override void Update()
    {
        var player = PlayerController.Instance;
        if (player == null || player.Dead) return;

        _timer -= Time.DeltaTime;
        if (_timer > 0f) return;
        _timer = fireInterval;

        EnemyController target = null;
        float best = float.MaxValue;
        Vector3f pp = player.Transform.Position;
        for (int i = 0; i < EnemyController.All.Count; i++)
        {
            var e = EnemyController.All[i];
            if (e == null || e.Dead) continue;
            Vector3f ep = e.Transform.Position;
            float dx = ep.x - pp.x;
            float dy = ep.y - pp.y;
            float d = dx * dx + dy * dy;
            if (d < best)
            {
                best = d;
                target = e;
            }
        }
        if (target == null) return;

        Vector3f tp = target.Transform.Position;
        ProjectileController.Fire(pp.x, pp.y - GameConst.BodyOffsetY, tp.x, tp.y - GameConst.BodyOffsetY, damage);
    }
}
