namespace GreenCake.TiledImporter;

using System.Collections.Generic;
using System.Text.Json.Serialization;

public class TiledMapData
{
    [JsonPropertyName("width")]
    public int Width { get; set; }

    [JsonPropertyName("height")]
    public int Height { get; set; }

    [JsonPropertyName("tilewidth")]
    public int TileWidth { get; set; }

    [JsonPropertyName("tileheight")]
    public int TileHeight { get; set; }

    [JsonPropertyName("tilesets")]
    public List<TiledTilesetEntry> Tilesets { get; set; } = new();

    [JsonPropertyName("layers")]
    public List<TiledLayerData> Layers { get; set; } = new();
}

public class TiledTilesetEntry
{
    [JsonPropertyName("firstgid")]
    public int FirstGid { get; set; }

    [JsonPropertyName("source")]
    public string? Source { get; set; }

    [JsonPropertyName("name")]
    public string? Name { get; set; }

    [JsonPropertyName("tilewidth")]
    public int? TileWidth { get; set; }

    [JsonPropertyName("tileheight")]
    public int? TileHeight { get; set; }

    [JsonPropertyName("columns")]
    public int? Columns { get; set; }

    [JsonPropertyName("tilecount")]
    public int? TileCount { get; set; }

    [JsonPropertyName("image")]
    public string? Image { get; set; }

    [JsonPropertyName("imagewidth")]
    public int? ImageWidth { get; set; }

    [JsonPropertyName("imageheight")]
    public int? ImageHeight { get; set; }

    [JsonIgnore]
    public bool IsEmbedded => string.IsNullOrEmpty(Source);
}

public class TiledLayerData
{
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
    public float Opacity { get; set; } = 1.0f;

    [JsonPropertyName("offsetx")]
    public int OffsetX { get; set; }

    [JsonPropertyName("offsety")]
    public int OffsetY { get; set; }

    [JsonPropertyName("data")]
    public uint[]? Data { get; set; }

    [JsonPropertyName("encoding")]
    public string? Encoding { get; set; }

    [JsonPropertyName("compression")]
    public string? Compression { get; set; }
}
