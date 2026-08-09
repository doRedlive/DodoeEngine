namespace OnlyOne;

using GreenCake;
using System;
using System.Collections.Generic;

public class EnemyController : CakeBehaviour
{
    public static readonly List<EnemyController> All = new List<EnemyController>();

    public float hp;
    public float radius = 2.5f;
    public float speed = 2.5f;
    public float stopDistance = 6.5f;

    private bool _dead;

    public bool Dead => _dead;

    public override void Awake()
    {
        All.Add(this);
    }

    public override void Update()
    {
        if (_dead) return;
        var player = PlayerController.Instance;
        if (player == null || player.Dead) return;

        Vector3f ep = Transform.Position;
        Vector3f pp = player.Transform.Position;
        float dx = pp.x - ep.x;
        float dy = pp.y - ep.y;
        float dist = (float)Math.Sqrt(dx * dx + dy * dy);
        if (dist <= stopDistance) return;

        dx /= dist;
        dy /= dist;
        ep.x += dx * speed * Time.DeltaTime;
        ep.y += dy * speed * Time.DeltaTime;
        Transform.Position = ep;
    }

    public void TakeDamage(float amount)
    {
        if (_dead) return;
        hp -= amount;
        if (hp <= 0f)
        {
            _dead = true;
            GameObject.Destroy(GameObject);
        }
    }

    public override void OnDestroy()
    {
        All.Remove(this);
    }

    public static EnemyController Create(Sprite sprite, Color color, float x, float y, float hp, float speed)
    {
        var go = GameObject.Create("Enemy");
        var sr = go.AddComponent<SpriteRendererComponent>();
        sr.Pivot = new Vector2f(0.5f, 0.5f);
        sr.Depth = 0.0f;
        sr.Color = color;
        sr.Sprite = sprite;
        go.Transform.Scale = new Vector3f(GameConst.BodyScale, GameConst.BodyScale, 1.0f);
        go.Transform.Position = new Vector3f(x, y, 0.0f);

        var e = go.AddComponent<EnemyController>();
        e.hp = hp;
        e.speed = speed;
        return e;
    }
}
