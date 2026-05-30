// do@Redlive
// Tiled Map Editor JSON format POCO classes.
// These model the external Tiled format and are separate from GreenCake.Tilemap types.

using System.Text.Json.Serialization;

namespace TiledLoader;

internal class TiledJsonMap
{
    [JsonPropertyName("width")]
    public int Width { get; set; }

    [JsonPropertyName("height")]
    public int Height { get; set; }

    [JsonPropertyName("tilewidth")]
    public int TileWidth { get; set; }

    [JsonPropertyName("tileheight")]
    public int TileHeight { get; set; }

    [JsonPropertyName("orientation")]
    public string Orientation { get; set; } = "orthogonal";

    [JsonPropertyName("tilesets")]
    public TiledJsonTileset[] Tilesets { get; set; } = [];

    [JsonPropertyName("layers")]
    public TiledJsonLayer[] Layers { get; set; } = [];

    [JsonPropertyName("properties")]
    public TiledJsonProperty[] Properties { get; set; } = [];
}

internal class TiledJsonTileset
{
    [JsonPropertyName("firstgid")]
    public int FirstGid { get; set; }

    [JsonPropertyName("source")]
    public string Source { get; set; } = string.Empty;

    [JsonPropertyName("name")]
    public string Name { get; set; } = string.Empty;

    [JsonPropertyName("image")]
    public string Image { get; set; } = string.Empty;

    [JsonPropertyName("imagewidth")]
    public int ImageWidth { get; set; }

    [JsonPropertyName("imageheight")]
    public int ImageHeight { get; set; }

    [JsonPropertyName("tilewidth")]
    public int TileWidth { get; set; }

    [JsonPropertyName("tileheight")]
    public int TileHeight { get; set; }

    [JsonPropertyName("columns")]
    public int Columns { get; set; }

    [JsonPropertyName("tilecount")]
    public int TileCount { get; set; }

    [JsonPropertyName("spacing")]
    public int Spacing { get; set; }

    [JsonPropertyName("margin")]
    public int Margin { get; set; }
}

internal class TiledJsonLayer
{
    [JsonPropertyName("id")]
    public int Id { get; set; }

    [JsonPropertyName("name")]
    public string Name { get; set; } = string.Empty;

    [JsonPropertyName("type")]
    public string Type { get; set; } = "tilelayer";

    [JsonPropertyName("width")]
    public int Width { get; set; }

    [JsonPropertyName("height")]
    public int Height { get; set; }

    [JsonPropertyName("visible")]
    public bool Visible { get; set; } = true;

    [JsonPropertyName("opacity")]
    public double Opacity { get; set; } = 1.0;

    [JsonPropertyName("x")]
    public int X { get; set; }

    [JsonPropertyName("y")]
    public int Y { get; set; }

    [JsonPropertyName("offsetx")]
    public int OffsetX { get; set; }

    [JsonPropertyName("offsety")]
    public int OffsetY { get; set; }

    [JsonPropertyName("data")]
    public uint[] Data { get; set; } = [];
}

internal class TiledJsonProperty
{
    [JsonPropertyName("name")]
    public string Name { get; set; } = string.Empty;

    [JsonPropertyName("type")]
    public string Type { get; set; } = "string";

    [JsonPropertyName("value")]
    public object Value { get; set; } = string.Empty;
}
