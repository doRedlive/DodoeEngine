namespace OnlyOne;

using GreenCake;

public class GameControllerSystem : CakeSystem
{
    private GameObject _player;
    private GameObject _mapImporter;

    public override void OnCreate()
    {
        // _player = GameObject.Create("Player");        
        // _player.AddComponent<PlayerController>();
        // _player.AddComponent<SpriteRendererComponent>();

        _mapImporter = GameObject.Create("MapImporter");
        _mapImporter.AddComponent<MapImporter>();
    }

    public override void OnUpdate()
    {

    }

    public override void OnDestroy()
    {
        
    }
}