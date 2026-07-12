namespace OnlyOne;

using GreenCake;

public class TestSystem : CakeSystem
{

    private GameObject _testSprite;
    private SpriteRendererComponent _spriteRenderer; 

    public void Start()
    {
        _testSprite = GameObject.Create("GrmSprite");
        _spriteRenderer = _testSprite.AddComponent<SpriteRendererComponent>();
        _spriteRenderer.Texture = Texture.Load("textures/Preview.png");
    }
}