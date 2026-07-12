namespace OnlyOne;

using GreenCake;

public class GameControllerSystem : CakeSystem
{
    private GameObject _player;

    public void Start()
    {
        _player = GameObject.Create("Player");        
        _player.AddComponent<PlayerController>();
        // _player.AddComponent<SpriteRendererComponent>();
    }
}