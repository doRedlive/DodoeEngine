namespace OnlyOne;

using GreenCake;
using GreenCake.TiledImporter;
using GreenCake.Tilemap;

public class MapImportSystem : CakeSystem
{
    private bool _imported = false;

    public void Start()
    {

    }

    public void Update()
    {
        if (_imported) return;
        _imported = true;
        var tilemap = TiledImporter.ImportFromFile("Maps/level1.tmj");
        // tilemap.InstantiateToScene();
    }
}
