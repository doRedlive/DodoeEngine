namespace DesktopGarden;

using GreenCake;

public sealed class GardenOneShot : CakeBehaviour
{
    public float Lifetime = 1.0f;

    private float _elapsed;

    public override void Update()
    {
        _elapsed += Time.DeltaTime;
        if (_elapsed >= Lifetime)
            GameObject.Destroy(GameObject);
    }
}
