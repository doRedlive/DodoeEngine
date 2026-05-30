namespace GreenCake.Tilemap;

using System.Collections.Generic;

public class Tilemap
{
    public uint MapWidth { get; set; }
    public uint MapHeight { get; set; }
    public uint TileWidth { get; set; } = 16;
    public uint TileHeight { get; set; } = 16;
    public List<Tileset> Tilesets { get; set; } = new();
    public List<TileLayer> Layers { get; set; } = new();

    public Entity InstantiateToScene()
    {
        if (World.Current is null)
            return null;

        Entity mapEntity = World.Current.CreateEntity("Tilemap");

        InternalCalls.Native_TilemapSetData(
            mapEntity.ID,
            (int)MapWidth, (int)MapHeight,
            (int)TileWidth, (int)TileHeight);

        foreach (var tileset in Tilesets)
        {
            string json = System.Text.Json.JsonSerializer.Serialize(tileset);
            InternalCalls.Native_TilemapAddTileset(mapEntity.ID, json);
        }

        foreach (var layer in Layers)
        {
            Entity layerEntity = World.Current.CreateEntity(layer.Name);

            InternalCalls.Native_TileLayerSetData(
                layerEntity.ID,
                layer.Tiles,
                (int)layer.Width,
                (int)layer.Height,
                layer.Name,
                layer.Visible,
                layer.Opacity,
                layer.OffsetX,
                layer.OffsetY);

            InternalCalls.Native_EntitySetParent(layerEntity.ID, mapEntity.ID);
        }

        return mapEntity;
    }
}
