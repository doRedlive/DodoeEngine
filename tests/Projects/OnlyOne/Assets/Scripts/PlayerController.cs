namespace OnlyOne;

using GreenCake;

public class PlayerController : CakeBehaviour
{

    public float speed = 1.0f;

    private SpriteRendererComponent _spriteRenderer;

    public override void Awake()
    {
        _spriteRenderer = GetOrAddComponent<SpriteRendererComponent>();
        var tex = Resources.Load<Texture>("textures/Preview.png");
        Debug.Log($"[PlayerController] Resources.Load returned: {(tex != null ? $"id={tex.InstanceID}" : "NULL")}");
        Debug.Log($"[PlayerController] speed={speed}");
        // _spriteRenderer.Texture = tex;
        Debug.Log($"[PlayerController] Awake done, sprite texture set");
    }

    public override void Update()
    {

    }

}
