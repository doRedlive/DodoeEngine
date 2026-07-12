namespace OnlyOne;

using GreenCake;

public class PlayerController : CakeBehaviour
{

    private SpriteRendererComponent _spriteRenderer;

    public override void Awake()
    {
        _spriteRenderer = GetOrAddComponent<SpriteRendererComponent>();
        _spriteRenderer.Texture = Resources.Load<Texture>("textures/Preview.png");
        Debug.Log("PlayerController Awake");
    }

    public override void Update()
    {

    }

}
