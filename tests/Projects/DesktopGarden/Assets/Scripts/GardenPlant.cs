namespace DesktopGarden;

using GreenCake;
using System;

public sealed class GardenPlant : CakeBehaviour
{
    public string DefinitionId = "clover";
    public int SlotIndex;
    public float GrowthSeconds = 180.0f;
    public float GrowthElapsed;
    public float Moisture;
    public float Fertility;
    public bool Sown;

    private SpriteRendererComponent _renderer;
    private int _stage = 0;
    private float _dryTimer;
    private float _swayTimer;

    private static readonly string[] StageNames =
    {
        "seed", "sprout", "leaf", "stem", "flower", "seedpod"
    };

    public override void Awake()
    {
        _renderer = GetOrAddComponent<SpriteRendererComponent>();
        _renderer.Pivot = new Vector2f(0.5f, 1.0f);
        _renderer.Depth = 10.0f + SlotIndex * 0.01f;
        _renderer.Color = Color.White;
        SetStage(0);
    }

    public override void Update()
    {
        float dt = Time.DeltaTime;
        _swayTimer += dt;

        if (!Sown)
            return;

        _dryTimer += dt;
        if (_dryTimer >= 1.0f)
        {
            _dryTimer = 0.0f;
            Moisture = Math.Max(0.0f, Moisture - 0.006f);
        }

        if (_stage >= StageNames.Length - 1)
            return;

        float moistureFactor = Moisture > 0.05f ? 1.0f : 0.18f;
        float fertilityFactor = 0.65f + Math.Clamp(Fertility, 0.0f, 1.0f) * 0.7f;
        GrowthElapsed += dt * moistureFactor * fertilityFactor;

        int nextStage = (int)Math.Floor(GrowthElapsed / Math.Max(1.0f, GrowthSeconds / StageNames.Length));
        nextStage = Math.Clamp(nextStage, 0, StageNames.Length - 1);
        if (nextStage != _stage)
            SetStage(nextStage);

        if (_stage >= 3)
        {
            float sway = (float)Math.Sin(_swayTimer * 1.4f + SlotIndex) * 0.025f;
            Transform.Rotation = new Vector3f(0.0f, 0.0f, sway);
        }
    }

    public void Sow(float fertility)
    {
        Sown = true;
        GrowthElapsed = 0.0f;
        Moisture = 0.65f;
        Fertility = fertility;
        _stage = 0;
        SetStage(_stage);
    }

    public void Water()
    {
        if (!Sown)
            return;

        Moisture = Math.Min(1.0f, Moisture + 0.45f);
        _dryTimer = 0.0f;
    }

    public bool IsReadyToHarvest()
    {
        return Sown && _stage == StageNames.Length - 1;
    }

    public void Harvest()
    {
        if (!IsReadyToHarvest())
            return;

        Sown = false;
        GrowthElapsed = 0.0f;
        Moisture = 0.0f;
        Fertility = 0.0f;
        _stage = 0;
        SetStage(_stage);
    }

    private void SetStage(int stage)
    {
        _stage = Math.Clamp(stage, 0, StageNames.Length - 1);
        if (_renderer == null)
            return;

        string path = "Textures/Plants/Stages/" + DefinitionId + "_" + StageNames[_stage] + ".png";
        Sprite sprite = Resources.Load<Sprite>(path);
        if (sprite != null)
            _renderer.Sprite = sprite;
    }
}
