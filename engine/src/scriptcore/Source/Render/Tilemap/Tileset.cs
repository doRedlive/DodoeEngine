namespace GreenCake.Tilemap;

public class Tileset
{
    public string Name { get; set; } = string.Empty;
    public uint FirstGid { get; set; } = 1;
    public uint TileWidth { get; set; } = 16;
    public uint TileHeight { get; set; } = 16;
    public uint Columns { get; set; }
    public uint TileCount { get; set; }
    public string ImagePath { get; set; } = string.Empty;
    public uint TextureId { get; set; }
}
