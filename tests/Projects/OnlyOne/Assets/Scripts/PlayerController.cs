namespace OnlyOne;

using GreenCake;

public class PlayerController : CakeBehaviour
{

    private SpriteRendererComponent _spriteRenderer;

    public override void Awake()
    {
        // _spriteRenderer = GetOrAddComponent<SpriteRendererComponent>();
        // var tex = Resources.Load<Texture>("textures/Preview.png");
        // Debug.Log($"[PlayerController] Resources.Load returned: {(tex != null ? $"id={tex.InstanceID}" : "NULL")}");
        // _spriteRenderer.Texture = tex;
        // Debug.Log($"[PlayerController] Awake done, sprite texture set");
    }

    public override void Update()
    {

    }

}
