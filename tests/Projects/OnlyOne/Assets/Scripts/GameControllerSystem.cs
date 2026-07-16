namespace OnlyOne;

using GreenCake;

public class GameControllerSystem : CakeSystem
{
    private GameObject _player;

    public override void OnCreate()
    {
        _player = GameObject.Create("Player");        
        _player.AddComponent<PlayerController>();
        // _player.AddComponent<SpriteRendererComponent>();
    }
}