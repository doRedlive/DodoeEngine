namespace GreenCake.FlowerSea;

using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

public static class FlowerSea
{
    private struct Archetype
    {
        public string Path;
        public string Label;
        public int Count;
        public bool Grid;
        public float Spread;
        public float Spacing;
        public float ScaleMin;
        public float ScaleMax;
        public float ScaleMul;
        public float YOffset;

        public Archetype(string path, string label, int count, bool grid, float spread,
                         float spacing, float scaleMin, float scaleMax, float scaleMul, float yOffset)
        {
            Path = path;
            Label = label;
            Count = count;
            Grid = grid;
            Spread = spread;
            Spacing = spacing;
            ScaleMin = scaleMin;
            ScaleMax = scaleMax;
            ScaleMul = scaleMul;
            YOffset = yOffset;
        }
    }

    private static readonly Archetype[] Archetypes =
    {
        new("Models/flower/source/lavendar.fbx", "lavender", 36, true, 0f, 1.4f, 0.8f, 1.3f, 0.01f, -1.0065f),
        new("Models/flower (2)/source/Roses Orange.fbx", "rose", 10, false, 14f, 0f, 0.6f, 1.2f, 1f, 0f),
        new("Models/flower (3)/scene.gltf", "flower3", 8, false, 14f, 0f, 0.7f, 1.2f, 1f, 0f),
        new("Models/flower (4)/scene.gltf", "flower4", 8, false, 14f, 0f, 0.7f, 1.2f, 1f, 0f),
        new("Models/flower (1)/scene.gltf", "flower1", 6, false, 12f, 0f, 0.8f, 1.2f, 0.01f, -1.0065f),
        new("Models/flower (5)/scene.gltf", "flower5", 4, false, 12f, 0f, 0.9f, 1.3f, 1f, 0f),
        new("Models/grass/source/Grass.fbx", "grass", 12, false, 16f, 0f, 1.0f, 2.0f, 1f, 0f),
    };

    private const string PrefabDir = "Prefabs/flower_sea";

    [ToolMenuItem("FlowerSea/Export Archetype Prefabs")]
    public static void ExportArchetypePrefabs()
    {
        var seen = new HashSet<string>(CollectUuids());
        int exported = 0;

        foreach (var archetype in Archetypes)
        {
            string fullPath = FilePath.Resolve(archetype.Path);
            if (!File.Exists(fullPath))
            {
                Debug.LogError($"[FlowerSea] Model not found: {fullPath}");
                continue;
            }

            NativeCalls.Native_SceneImportModel(fullPath);

            var uuids = CollectUuids();
            string top = FindNewTopNode(uuids, seen);
            if (top == null)
            {
                Debug.LogError($"[FlowerSea] Failed to locate imported instance for '{archetype.Label}'.");
                continue;
            }

            string prefabPath = $"{PrefabDir}/{archetype.Label}.prefab";
            ulong topId = ulong.Parse(top, System.Globalization.CultureInfo.InvariantCulture);
            if (!NativeCalls.Native_SceneExportPrefab(topId, prefabPath))
            {
                Debug.LogError($"[FlowerSea] Failed to export prefab '{prefabPath}'.");
                continue;
            }
            exported++;

            var subtree = CollectSubtreeUuids(top);
            foreach (string uuid in subtree)
            {
                NativeCalls.Native_DestroyEntity(ulong.Parse(uuid, System.Globalization.CultureInfo.InvariantCulture));
            }
            foreach (string uuid in subtree) seen.Add(uuid);
        }

        Debug.Log($"[FlowerSea] Exported {exported}/{Archetypes.Length} prefabs to '{PrefabDir}'.");
    }

    [ToolMenuItem("FlowerSea/Build Flower Sea")]
    public static void BuildFlowerSea()
    {
        var random = new Random(20260904);
        var seen = new HashSet<string>(CollectUuids());

        foreach (var archetype in Archetypes)
        {
            string fullPath = FilePath.Resolve(archetype.Path);
            if (!File.Exists(fullPath))
            {
                Debug.LogError($"[FlowerSea] Model not found: {fullPath}");
                continue;
            }

            int placed = 0;
            for (int i = 0; i < archetype.Count; i++)
            {
                string prefabPath = FilePath.Resolve($"{PrefabDir}/{archetype.Label}.prefab");
                if (File.Exists(prefabPath))
                    NativeCalls.Native_SceneImportPrefab(prefabPath);
                else
                    NativeCalls.Native_SceneImportModel(fullPath);

                var uuids = CollectUuids();
                string top = FindNewTopNode(uuids, seen);
                if (top == null)
                {
                    Debug.LogError("[FlowerSea] Failed to locate imported instance root.");
                    break;
                }
                seen.UnionWith(uuids);

                float x, z;
                if (archetype.Grid)
                {
                    int side = (int)Math.Ceiling(Math.Sqrt(archetype.Count));
                    int gx = placed % side;
                    int gz = placed / side;
                    x = (gx - (side - 1) * 0.5f) * archetype.Spacing;
                    z = (gz - (side - 1) * 0.5f) * archetype.Spacing;
                }
                else
                {
                    double ang = random.NextDouble() * Math.PI * 2.0;
                    double rad = archetype.Spread * Math.Sqrt(random.NextDouble());
                    x = (float)(Math.Cos(ang) * rad);
                    z = (float)(Math.Sin(ang) * rad);
                }

                float jitter = (float)(archetype.ScaleMin +
                                       random.NextDouble() * (archetype.ScaleMax - archetype.ScaleMin));
                float scale = archetype.ScaleMul * jitter;
                float yaw = (float)(random.NextDouble() * 360.0);
                float y = archetype.YOffset * (scale / archetype.ScaleMul);

                ulong entityId = ulong.Parse(top, System.Globalization.CultureInfo.InvariantCulture);
                NativeCalls.TransformComponent_SetPosition(entityId, x, y, z);
                NativeCalls.TransformComponent_SetRotation(entityId, 0f, yaw, 0f);
                NativeCalls.TransformComponent_SetScale(entityId, scale, scale, scale);
                placed++;
            }

            Debug.Log($"[FlowerSea] Placed {placed}/{archetype.Count} '{archetype.Label}'.");
        }

        if (NativeCalls.Native_WorldSaveActiveScene())
            Debug.Log("[FlowerSea] Build finished and scene saved.");
        else
            Debug.LogError("[FlowerSea] Build finished but scene save failed.");
    }

    private static List<string> CollectUuids()
    {
        var result = new List<string>();
        string json = NativeCalls.Native_SceneDumpHierarchy();
        if (string.IsNullOrEmpty(json)) return result;

        try
        {
            using var doc = JsonDocument.Parse(json);
            foreach (var element in doc.RootElement.EnumerateArray())
            {
                if (element.TryGetProperty("uuid", out var uuid))
                    result.Add(uuid.GetRawText());
            }
        }
        catch (Exception ex)
        {
            Debug.LogError($"[FlowerSea] Failed to parse hierarchy dump: {ex.Message}");
        }
        return result;
    }

    private static string FindNewTopNode(List<string> uuids, HashSet<string> seen)
    {
        var parentOf = new Dictionary<string, string>();
        var entries = new List<(string uuid, string parent)>();
        string json = NativeCalls.Native_SceneDumpHierarchy();

        try
        {
            using var doc = JsonDocument.Parse(json);
            foreach (var element in doc.RootElement.EnumerateArray())
            {
                if (!element.TryGetProperty("uuid", out var uuidProp)) continue;
                string uuid = uuidProp.GetRawText();
                string parent = element.TryGetProperty("parent", out var parentProp)
                    ? parentProp.GetRawText()
                    : null;
                parentOf[uuid] = parent;
                entries.Add((uuid, parent));
            }
        }
        catch (Exception ex)
        {
            Debug.LogError($"[FlowerSea] Failed to parse hierarchy dump: {ex.Message}");
            return null;
        }

        string best = null;
        int bestDepth = int.MaxValue;
        foreach (var (uuid, _) in entries)
        {
            if (seen.Contains(uuid)) continue;

            int depth = 0;
            string p = parentOf.TryGetValue(uuid, out var cur) ? cur : null;
            while (p != null && parentOf.ContainsKey(p))
            {
                depth++;
                p = parentOf.TryGetValue(p, out var next) ? next : null;
            }

            if (depth < bestDepth)
            {
                bestDepth = depth;
                best = uuid;
            }
        }
        return best;
    }

    private static List<string> CollectSubtreeUuids(string topUuid)
    {
        var result = new List<string> { topUuid };
        string json = NativeCalls.Native_SceneDumpHierarchy();
        if (string.IsNullOrEmpty(json)) return result;

        try
        {
            using var doc = JsonDocument.Parse(json);
            var parentOf = new Dictionary<string, string>();
            foreach (var element in doc.RootElement.EnumerateArray())
            {
                if (!element.TryGetProperty("uuid", out var uuidProp)) continue;
                parentOf[uuidProp.GetRawText()] =
                    element.TryGetProperty("parent", out var parentProp) ? parentProp.GetRawText() : null;
            }

            foreach (var pair in parentOf)
            {
                string p = pair.Value;
                while (p != null)
                {
                    if (p == topUuid)
                    {
                        result.Add(pair.Key);
                        break;
                    }
                    parentOf.TryGetValue(p, out p);
                }
            }
        }
        catch (Exception ex)
        {
            Debug.LogError($"[FlowerSea] Failed to parse hierarchy dump: {ex.Message}");
        }
        return result;
    }
}
