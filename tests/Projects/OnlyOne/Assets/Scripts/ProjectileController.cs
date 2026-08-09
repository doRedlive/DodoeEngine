namespace OnlyOne;

using GreenCake;
using System;
using System.Collections.Generic;

public class ProjectileController : CakeBehaviour
{
    public static readonly List<ProjectileController> All = new List<ProjectileController>();

    public float dx;
    public float dy;
    public float speed = 22.0f;
    public float damage = 1.0f;
    public float radius = 0.6f;
    public float maxAge = 2.5f;

    private bool _dead;
    private float _age;

    public override void Awake()
    {
        All.Add(this);
    }

    public override void Update()
    {
        if (_dead) return;
        _age += Time.DeltaTime;

        Vector3f p = Transform.Position;
        p.x += dx * speed * Time.DeltaTime;
        p.y += dy * speed * Time.DeltaTime;
        Transform.Position = p;

        if (_age >= maxAge)
        {
            KillSelf();
            return;
        }

        for (int i = 0; i < EnemyController.All.Count; i++)
        {
            var e = EnemyController.All[i];
            if (e == null || e.Dead) continue;
            Vector3f ep = e.Transform.Position;
            ep.y -= GameConst.BodyOffsetY;
            float ox = ep.x - p.x;
            float oy = ep.y - p.y;
            float rr = e.radius + radius;
            if (ox * ox + oy * oy <= rr * rr)
            {
                e.TakeDamage(damage);
                KillSelf();
                return;
            }
        }
    }

    private void KillSelf()
    {
        _dead = true;
        GameObject.Destroy(GameObject);
    }

    public override void OnDestroy()
    {
        All.Remove(this);
    }

    public static void Fire(float x, float y, float tx, float ty, float damage)
    {
        float dx = tx - x;
        float dy = ty - y;
        float len = (float)Math.Sqrt(dx * dx + dy * dy);
        if (len < 0.001f) return;
        dx /= len;
        dy /= len;

        var go = GameObject.Create("Bullet");
        var rect = go.AddComponent<RectRendererComponent>();
        rect.Size = new Vector2f(2.4f, 3.2f);
        rect.Color = new Color(1.0f, 0.9f, 0.3f, 1.0f);
        go.Transform.Position = new Vector3f(x, y, 0.1f);

        var b = go.AddComponent<ProjectileController>();
        b.dx = dx;
        b.dy = dy;
        b.damage = damage;
    }
}
