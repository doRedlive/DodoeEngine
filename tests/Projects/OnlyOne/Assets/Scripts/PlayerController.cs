namespace OnlyOne;

using GreenCake;
using System;

public class PlayerController : CakeBehaviour
{
    public float speed = 8.0f;
    public float maxHp = 100.0f;
    public float hp = 100.0f;
    public float contactRadius = 2.5f;

    public bool Dead { get; private set; } = false;

    public static PlayerController Instance;

    private float _hitTimer;

    private const string kPartDir =
        "Pictures/Free 2D Animated Vector Game Character Sprites/Animated body parts";
    private Sprite[] _walkFrames;
    private int _frameIndex;
    private float _animTimer;
    private SpriteRendererComponent _sr;

    public override void Awake()
    {
        Instance = this;
        hp = maxHp;
        Dead = false;
        _hitTimer = 0f;

        _sr = GetOrAddComponent<SpriteRendererComponent>();
        _sr.Pivot = new Vector2f(0.5f, 0.5f);
        _sr.Depth = 0.0f;
        Transform.Scale = new Vector3f(GameConst.BodyScale, GameConst.BodyScale, 1.0f);

        _walkFrames = LoadFrames("Bodies/body1/walk_{0}.png");
        if (_walkFrames.Length > 0)
        {
            _sr.Sprite = _walkFrames[0];
        }

        SetupInput();
    }

    private static void SetupInput()
    {
        Input.RegisterActionMap("Gameplay");
        Input.RegisterAction("Gameplay", "Move", InputActionValueType.Axis2D);
        Input.BindKey2D("Gameplay", "Move", KeyCode.W, 0f, 1f);
        Input.BindKey2D("Gameplay", "Move", KeyCode.S, 0f, -1f);
        Input.BindKey2D("Gameplay", "Move", KeyCode.A, -1f, 0f);
        Input.BindKey2D("Gameplay", "Move", KeyCode.D, 1f, 0f);
    }

    private Sprite[] LoadFrames(string pattern)
    {
        var list = new System.Collections.Generic.List<Sprite>();
        for (int i = 0; i < 8; i++)
        {
            try
            {
                var s = Resources.Load<Sprite>(kPartDir + "/" + string.Format(pattern, i));
                if (s != null) list.Add(s);
            }
            catch (Exception)
            {
                break;
            }
        }
        return list.ToArray();
    }

    public override void Update()
    {
        if (Dead) return;

        float dt = Time.DeltaTime;
        Vector3f p = Transform.Position;

        Vector2f move = Input.GetActionVector2("Gameplay/Move");
        float dx = move.x;
        float dy = move.y;

        bool moving = (dx != 0f || dy != 0f);
        if (moving)
        {
            float len = (float)Math.Sqrt(dx * dx + dy * dy);
            dx /= len; dy /= len;
            p.x += dx * speed * dt;
            p.y += dy * speed * dt;
        }

        float boundX = 60f, boundY = 35f;
        if (p.x > boundX) p.x = boundX;
        if (p.x < -boundX) p.x = -boundX;
        if (p.y > boundY) p.y = boundY;
        if (p.y < -boundY) p.y = -boundY;

        Transform.Position = p;

        if (_walkFrames.Length > 0)
        {
            float frameDuration = moving ? 0.10f : 0.16f;
            _animTimer += dt;
            if (_animTimer >= frameDuration)
            {
                _animTimer = 0f;
                _frameIndex = (_frameIndex + 1) % _walkFrames.Length;
                _sr.Sprite = _walkFrames[_frameIndex];
            }
        }

        CheckContactDamage(p);
    }

    private void CheckContactDamage(Vector3f p)
    {
        _hitTimer -= Time.DeltaTime;
        if (_hitTimer > 0f) return;
        for (int i = 0; i < EnemyController.All.Count; i++)
        {
            var e = EnemyController.All[i];
            if (e == null || e.Dead) continue;
            Vector3f ep = e.Transform.Position;
            float dx = ep.x - p.x;
            float dy = ep.y - p.y;
            float rr = contactRadius + e.radius;
            if (dx * dx + dy * dy <= rr * rr)
            {
                _hitTimer = 0.7f;
                TakeDamage(10f);
                break;
            }
        }
    }

    public void TakeDamage(float amount)
    {
        if (Dead) return;
        hp -= amount;
        if (hp <= 0f)
        {
            hp = 0f;
            Dead = true;
        }
    }
}
