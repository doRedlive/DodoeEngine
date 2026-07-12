namespace OnlyOne;

using GreenCake;
using GreenCake.TiledImporter;
using GreenCake.Tilemap;

public class MapImportSystem : CakeSystem
{
    private bool _imported;

    public void Start()
    {
        if (_imported) return;
        _imported = true;

        var tilemap = TiledImporter.ImportFromFile("Maps/level1.tmj");
        tilemap.InstantiateToScene();
    }
}
