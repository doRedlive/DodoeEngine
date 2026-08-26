namespace GreenCake.TiledImporter;

using System;
using System.IO;
using System.Text.Json;
using GreenCake.Tilemap;

public static class TiledImporter
{
    public static Tilemap ImportFromFile(string jsonFilePath)
    {
        string fullPath = FilePath.Resolve(jsonFilePath);
        string jsonText = File.ReadAllText(fullPath);
        return ImportFromJson(jsonText, Path.GetDirectoryName(fullPath) ?? ".");
    }

    public static Tilemap ImportFromJson(string json, string baseDir)
    {
        var data = JsonSerializer.Deserialize<TiledMapData>(json);
        if (data == null)
            throw new InvalidOperationException("[TiledImporter] Failed to deserialize map JSON.");

        var tilemap = new Tilemap
        {
            MapWidth = (uint)data.Width,
            MapHeight = (uint)data.Height,
            TileWidth = (uint)data.TileWidth,
            TileHeight = (uint)data.TileHeight
        };

        foreach (var entry in data.Tilesets)
        {
            Tileset? tileset = entry.IsEmbedded
                ? BuildTilesetFromEntry(entry, baseDir)
                : ResolveExternalTileset(entry, baseDir);

            if (tileset != null)
                tilemap.Tilesets.Add(tileset);
        }

        foreach (var layer in data.Layers)
        {
            if (layer.Type != "tilelayer")
                continue;

            tilemap.Layers.Add(new TileLayer
            {
                Name = layer.Name,
                Width = (uint)layer.Width,
                Height = (uint)layer.Height,
                Tiles = layer.Data ?? Array.Empty<uint>(),
                Visible = layer.Visible,
                Opacity = layer.Opacity,
                OffsetX = layer.OffsetX,
                OffsetY = layer.OffsetY
            });
        }

        return tilemap;
    }

    private static Tileset BuildTilesetFromEntry(TiledTilesetEntry entry, string baseDir)
    {
        string imageRelPath = entry.Image ?? string.Empty;
        string imageAbsPath = Path.GetFullPath(Path.Combine(baseDir, imageRelPath));

        return new Tileset
        {
            Name = entry.Name ?? "Tileset",
            FirstGid = (uint)entry.FirstGid,
            TileWidth = (uint)(entry.TileWidth ?? 16),
            TileHeight = (uint)(entry.TileHeight ?? 16),
            Columns = (uint)(entry.Columns ?? 0),
            TileCount = (uint)(entry.TileCount ?? 0),
            ImagePath = imageAbsPath
        };
    }

    private static Tileset? ResolveExternalTileset(TiledTilesetEntry entry, string baseDir)
    {
        string tsxPath = Path.GetFullPath(Path.Combine(baseDir, entry.Source!));

        if (!File.Exists(tsxPath))
        {
            Debug.Log($"[TiledImporter] TSX not found: {tsxPath}");
            return null;
        }

        string tsxJson = File.ReadAllText(tsxPath);
        var tsxEntry = JsonSerializer.Deserialize<TiledTilesetEntry>(tsxJson);
        if (tsxEntry == null)
            return null;

        tsxEntry.FirstGid = entry.FirstGid;
        string tsxDir = Path.GetDirectoryName(tsxPath) ?? baseDir;
        var tileset = BuildTilesetFromEntry(tsxEntry, tsxDir);
        if (tileset != null)
            tileset.Source = tsxPath;
        return tileset;
    }

    [ToolMenuItem("Tiled/Import level1.tmj into Scene")]
    public static void ImportLevel1IntoScene()
    {
        const string relativePath = "Maps/level1.tmj";
        string fullPath = FilePath.Resolve(relativePath);
        if (!File.Exists(fullPath))
        {
            Debug.LogError($"[TiledImporter] Map file not found: {fullPath}");
            return;
        }

        try
        {
            Tilemap tilemap = ImportFromFile(fullPath);
            tilemap.InstantiateToScene();
            Debug.Log($"[TiledImporter] Imported '{fullPath}': {tilemap.MapWidth}x{tilemap.MapHeight}, " +
                      $"{tilemap.Tilesets.Count} tileset(s), {tilemap.Layers.Count} layer(s).");
        }
        catch (Exception ex)
        {
            Debug.LogError($"[TiledImporter] Import failed: {ex}");
        }
    }
}
