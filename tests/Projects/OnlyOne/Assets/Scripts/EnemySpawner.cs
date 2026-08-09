namespace OnlyOne;

using GreenCake;
using System;
using System.Collections.Generic;

public class EnemySpawner : CakeBehaviour
{
    public const string kPartBase =
        "Pictures/Free 2D Animated Vector Game Character Sprites/Animated body parts";

    public float spawnInterval = 1.0f;
    public float arenaX = 60.0f;
    public float arenaY = 35.0f;
    public int maxEnemies = 40;

    private Sprite[] _pool;
    private readonly Color[] _tints =
    {
        new Color(1.0f, 0.35f, 0.3f, 1.0f),
        new Color(0.6f, 0.4f, 1.0f, 1.0f),
        new Color(0.3f, 0.9f, 0.5f, 1.0f),
        new Color(0.95f, 0.7f, 0.2f, 1.0f),
    };
    private readonly System.Random _rng = new System.Random();
    private float _timer;
    private float _elapsed;

    public override void Awake()
    {
        _pool = LoadPool();
        _timer = 0.5f;
        _elapsed = 0f;
    }

    public override void Update()
    {
        var player = PlayerController.Instance;
        if (player == null || player.Dead) return;

        _elapsed += Time.DeltaTime;
        _timer -= Time.DeltaTime;
        if (_timer > 0f) return;
        _timer = spawnInterval;

        int count = 1;
        if (_elapsed > 40f) count = 2;
        if (_elapsed > 90f) count = 3;

        for (int i = 0; i < count; i++)
        {
            if (EnemyController.All.Count >= maxEnemies) return;
            SpawnOne(player);
        }
    }

    private void SpawnOne(PlayerController player)
    {
        if (_pool.Length == 0) return;

        float x = 0f, y = 0f;
        int edge = _rng.Next(4);
        if (edge == 0) { x = -arenaX - 2f; y = RandRange(-arenaY, arenaY); }
        else if (edge == 1) { x = arenaX + 2f; y = RandRange(-arenaY, arenaY); }
        else if (edge == 2) { y = -arenaY - 2f; x = RandRange(-arenaX, arenaX); }
        else { y = arenaY + 2f; x = RandRange(-arenaX, arenaX); }

        float hp = 3f + _elapsed * 0.05f;
        if (hp > 12f) hp = 12f;
        float speed = 2.2f + _elapsed * 0.006f;
        if (speed > 4.2f) speed = 4.2f;

        EnemyController.Create(_pool[_rng.Next(_pool.Length)], _tints[_rng.Next(_tints.Length)], x, y, hp, speed);
    }

    private Sprite[] LoadPool()
    {
        var list = new List<Sprite>();
        for (int i = 0; i < 8; i++)
        {
            try
            {
                var s = Resources.Load<Sprite>(kPartBase + "/Bodies/body1/walk_" + i + ".png");
                if (s != null) list.Add(s);
            }
            catch (Exception)
            {
            }
        }
        if (list.Count == 0)
        {
            var fb = Resources.Load<Sprite>(kPartBase + "/Bodies/body1/walk_0.png");
            if (fb != null) list.Add(fb);
        }
        return list.ToArray();
    }

    private float RandRange(float min, float max)
    {
        return min + (float)_rng.NextDouble() * (max - min);
    }
}
