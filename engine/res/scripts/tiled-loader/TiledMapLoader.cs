// do@Redlive
// Tiled Map Editor JSON (.tmj) loader.
// Converts Tiled JSON format into GreenCake.Tilemap.Tilemap for scene instantiation.

using System;
using System.IO;
using System.Text.Json;
using GreenCake.Tilemap;

namespace TiledLoader;

public static class TiledMapLoader
{
    /// <summary>
    /// Load a Tiled JSON map file (.tmj) and return a GreenCake Tilemap.
    /// </summary>
    public static Tilemap Load(string tiledJsonPath)
    {
        string fullPath = Path.GetFullPath(tiledJsonPath);
        if (!File.Exists(fullPath))
        {
            GreenCake.Debug.Log($"[TiledLoader] File not found: {fullPath}");
            return null;
        }

        string json = File.ReadAllText(fullPath);
        var options = new JsonSerializerOptions
        {
            PropertyNameCaseInsensitive = true
        };

        TiledJsonMap tiledMap;
        try
        {
            tiledMap = JsonSerializer.Deserialize<TiledJsonMap>(json, options);
        }
        catch (JsonException ex)
        {
            GreenCake.Debug.Log($"[TiledLoader] JSON parse error: {ex.Message}");
            return null;
        }

        if (tiledMap is null)
        {
            GreenCake.Debug.Log("[TiledLoader] Failed to deserialize map.");
            return null;
        }

        string baseDir = Path.GetDirectoryName(fullPath);

        var tilemap = new Tilemap
        {
            MapWidth = (uint)tiledMap.Width,
            MapHeight = (uint)tiledMap.Height,
            TileWidth = (uint)tiledMap.TileWidth,
            TileHeight = (uint)tiledMap.TileHeight
        };

        // Convert tilesets
        foreach (var ts in tiledMap.Tilesets)
        {
            Tileset tileset;
            if (!string.IsNullOrEmpty(ts.Source))
            {
                tileset = LoadExternalTileset(baseDir, ts);
            }
            else
            {
                tileset = ConvertEmbeddedTileset(ts);
            }

            if (tileset is not null)
            {
                tilemap.Tilesets.Add(tileset);
            }
        }

        // Convert layers (tile layers only)
        foreach (var layer in tiledMap.Layers)
        {
            if (layer.Type != "tilelayer")
            {
                continue;
            }

            tilemap.Layers.Add(new TileLayer
            {
                Name = layer.Name,
                Width = (uint)layer.Width,
                Height = (uint)layer.Height,
                Tiles = layer.Data ?? [],
                Visible = layer.Visible,
                Opacity = (float)layer.Opacity,
                OffsetX = layer.OffsetX,
                OffsetY = layer.OffsetY
            });
        }

        return tilemap;
    }

    private static Tileset LoadExternalTileset(string baseDir, TiledJsonTileset refTs)
    {
        string tsxPath = Path.Combine(baseDir, refTs.Source);
        if (!File.Exists(tsxPath))
        {
            GreenCake.Debug.Log($"[TiledLoader] External tileset not found: {tsxPath}");
            return null;
        }

        string ext = Path.GetExtension(tsxPath).ToLowerInvariant();
        if (ext == ".tsx")
        {
            GreenCake.Debug.Log($"[TiledLoader] XML-based .tsx tilesets are not yet supported. " +
                "Use Tiled's JSON tileset export or embed tileset data in the map JSON. " +
                $"Skipping: {refTs.Source}");
            return null;
        }

        string json = File.ReadAllText(tsxPath);
        var options = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };

        try
        {
            var ts = JsonSerializer.Deserialize<TiledJsonTileset>(json, options);
            if (ts is null) return null;

            ts.FirstGid = refTs.FirstGid;
            return ConvertEmbeddedTileset(ts);
        }
        catch (JsonException ex)
        {
            GreenCake.Debug.Log($"[TiledLoader] TSX parse error: {ex.Message}");
            return null;
        }
    }

    private static Tileset ConvertEmbeddedTileset(TiledJsonTileset ts)
    {
        var tileset = new Tileset
        {
            Name = ts.Name,
            FirstGid = (uint)ts.FirstGid,
            TileWidth = (uint)(ts.TileWidth > 0 ? ts.TileWidth : 16),
            TileHeight = (uint)(ts.TileHeight > 0 ? ts.TileHeight : 16),
            TileCount = (uint)ts.TileCount,
            ImagePath = ts.Image
        };

        if (ts.Columns > 0)
        {
            tileset.Columns = (uint)ts.Columns;
        }
        else if (ts.ImageWidth > 0 && tileset.TileWidth > 0)
        {
            tileset.Columns = (uint)(ts.ImageWidth / tileset.TileWidth);
        }

        return tileset;
    }
}
