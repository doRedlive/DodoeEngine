namespace DesktopGarden;

using GreenCake;
using System;
using System.Collections.Generic;

public sealed class GardenController : CakeBehaviour
{
    public float dayLengthSeconds = 300.0f;
    public float startingFertility = 0.35f;

    private readonly List<GardenPlant> _plants = new();
    private SpriteRendererComponent _backgroundRenderer;
    private SpriteRendererComponent _weatherRenderer;
    private AudioSourceComponent _ambience;
    private Sprite _dayBackground;
    private Sprite _nightBackground;
    private Sprite _weatherOverlay;
    private float _clock;
    private float _fertility;
    private bool _night;
    private bool _raining;
    private float _statusTimer;
    private string _ambiencePath = "";

    private static bool _inputReady;

    public override void Awake()
    {
        _fertility = Math.Clamp(startingFertility, 0.0f, 1.0f);
        RegisterInput();
        LoadSprites();
        CreateBackdrop();
        CreatePlants();
        CreateAmbience();
        UpdateEnvironment(true);
    }

    public override void Update()
    {
        float dt = Time.DeltaTime;
        _clock += dt;
        _statusTimer += dt;

        if (Input.IsActionDown("Garden/Type"))
            _fertility = Math.Min(1.0f, _fertility + dt * 0.018f);
        else
            _fertility = Math.Max(0.0f, _fertility - dt * 0.0015f);

        float phase = dayLengthSeconds <= 1.0f ? 0.0f : (_clock % dayLengthSeconds) / dayLengthSeconds;
        bool night = phase >= 0.58f;
        bool rain = phase >= 0.32f && phase < 0.47f;
        if (night != _night || rain != _raining)
            UpdateEnvironment(false);

        foreach (GardenPlant plant in _plants)
            plant.Fertility = _fertility;

        if (Input.WasActionPressed("Garden/Interact"))
            HandleGardenClick(Input.GetMousePosition());

        if (_statusTimer >= 12.0f)
        {
            _statusTimer = 0.0f;
            Debug.Log("DesktopGarden fertility=" + _fertility.ToString("0.00") + " weather=" + (_raining ? "rain" : "clear") + " time=" + (_night ? "night" : "day"));
        }
    }

    private void RegisterInput()
    {
        if (_inputReady)
            return;

        Input.RegisterActionMap("Garden");
        Input.RegisterAction("Garden", "Interact", InputActionValueType.Button);
        Input.RegisterAction("Garden", "Type", InputActionValueType.Button);
        Input.BindMouseButton("Garden", "Interact", MouseCode.ButtonLeft);
        Input.BindKey("Garden", "Type", KeyCode.Space);
        Input.SetActionMapEnabled("Garden", true);
        _inputReady = true;
    }

    private void LoadSprites()
    {
        _dayBackground = Resources.Load<Sprite>("Textures/Background/background_day.png");
        _nightBackground = Resources.Load<Sprite>("Textures/Background/background_night.png");
        _weatherOverlay = Resources.Load<Sprite>("Textures/Weather/weather_overlay.png");
    }

    private void CreateBackdrop()
    {
        GameObject background = GameObject.Create("GardenBackground");
        background.Transform.Position = new Vector3f(0.0f, 0.0f, 5.0f);
        _backgroundRenderer = background.AddComponent<SpriteRendererComponent>();
        _backgroundRenderer.Pivot = new Vector2f(0.5f, 0.5f);
        _backgroundRenderer.Depth = -100.0f;
        _backgroundRenderer.Sprite = _dayBackground;

        GameObject weather = GameObject.Create("GardenWeather");
        weather.Transform.Position = new Vector3f(0.0f, 0.0f, 4.0f);
        _weatherRenderer = weather.AddComponent<SpriteRendererComponent>();
        _weatherRenderer.Pivot = new Vector2f(0.5f, 0.5f);
        _weatherRenderer.Depth = 100.0f;
        _weatherRenderer.Sprite = _weatherOverlay;
        _weatherRenderer.Color = new Color(0.8f, 0.9f, 1.0f, 0.0f);
    }

    private void CreatePlants()
    {
        string[] ids = { "clover", "clover", "moonflower", "clover", "moonflower" };
        float[] positions = { -5.0f, -2.5f, 0.0f, 2.5f, 5.0f };
        for (int i = 0; i < ids.Length; i++)
        {
            GameObject slot = GameObject.Create("PlantSlot_" + i);
            slot.Transform.Position = new Vector3f(positions[i], -2.25f, 0.0f);
            GardenPlant plant = slot.AddComponent<GardenPlant>();
            plant.DefinitionId = ids[i];
            plant.SlotIndex = i;
            plant.GrowthSeconds = ids[i] == "moonflower" ? 360.0f : 180.0f;
            _plants.Add(plant);
        }
    }

    private void CreateAmbience()
    {
        GameObject audioObject = GameObject.Create("GardenAmbience");
        _ambience = audioObject.AddComponent<AudioSourceComponent>();
        _ambience.Loop = true;
        _ambience.Volume = 0.22f;
        _ambience.PlayOnAwake = true;
    }

    private void UpdateEnvironment(bool force)
    {
        float phase = dayLengthSeconds <= 1.0f ? 0.0f : (_clock % dayLengthSeconds) / dayLengthSeconds;
        _night = phase >= 0.58f;
        _raining = phase >= 0.32f && phase < 0.47f;

        if (_backgroundRenderer != null)
            _backgroundRenderer.Sprite = _night ? _nightBackground : _dayBackground;
        if (_weatherRenderer != null)
            _weatherRenderer.Color = _raining ? new Color(0.8f, 0.9f, 1.0f, 0.16f) : new Color(1.0f, 1.0f, 1.0f, 0.0f);

        if (_ambience == null)
            return;

        string path = _raining ? "Audio/ambience_rain.wav" : (_night ? "Audio/ambience_night.wav" : "Audio/ambience_day.wav");
        AudioClip clip = AudioClip.Load(path);
        if (clip != null && (force || _ambiencePath != path))
        {
            _ambience.Clip = clip;
            _ambiencePath = path;
            _ambience.Play();
        }
    }

    private void HandleGardenClick(Vector2f mouse)
    {
        float worldX = (mouse.x / 1600.0f - 0.5f) * 12.8f;
        float worldY = (0.5f - mouse.y / 900.0f) * 7.2f;
        GardenPlant nearest = null;
        float nearestDistance = 999.0f;

        foreach (GardenPlant plant in _plants)
        {
            Vector3f p = plant.Transform.Position;
            float dx = p.x - worldX;
            float dy = p.y - worldY;
            float distance = dx * dx + dy * dy;
            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearest = plant;
            }
        }

        if (nearest == null || nearestDistance > 32.0f)
            return;

        if (!nearest.Sown)
        {
            nearest.Sow(_fertility);
            PlayOneShot("Audio/seed_plant.wav");
        }
        else if (nearest.IsReadyToHarvest())
        {
            nearest.Harvest();
            _fertility = Math.Min(1.0f, _fertility + 0.08f);
            PlayOneShot("Audio/harvest.wav");
        }
        else
        {
            nearest.Water();
            PlayOneShot("Audio/water_drop.wav");
        }
    }

    private void PlayOneShot(string path)
    {
        GameObject effect = GameObject.Create("GardenSfx");
        AudioSourceComponent source = effect.AddComponent<AudioSourceComponent>();
        source.Clip = AudioClip.Load(path);
        source.Volume = 0.35f;
        source.PlayOnAwake = false;
        GardenOneShot lifetime = effect.AddComponent<GardenOneShot>();
        lifetime.Lifetime = 1.5f;
        source.Play();
    }
}
