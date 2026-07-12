namespace GreenCake.Tilemap;

public class TileLayer
{
    public string Name { get; set; } = string.Empty;
    public uint Width { get; set; }
    public uint Height { get; set; }
    public uint[] Tiles { get; set; } = System.Array.Empty<uint>();
    public bool Visible { get; set; } = true;
    public float Opacity { get; set; } = 1.0f;
    public int OffsetX { get; set; }
    public int OffsetY { get; set; }
}
