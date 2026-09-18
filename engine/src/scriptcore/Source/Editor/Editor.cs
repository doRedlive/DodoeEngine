namespace GreenCake.Editor;

using System;

public enum PlayState
{
    Edit = 0,
    Playing = 1,
    Paused = 2,
}

public readonly struct EntityInfo
{
    public ulong Uuid { get; }
    public ulong Parent { get; }
    public string Name { get; }

    public EntityInfo(ulong uuid, ulong parent, string name)
    {
        Uuid = uuid;
        Parent = parent;
        Name = name;
    }

    public bool IsValid => Uuid != 0;
}

public static class EditorApplication
{
    public static bool IsAvailable => EditorNativeCalls.IsBound;

    public static bool IsReady => EditorNativeCalls.IsReady();

    public static PlayState State => (PlayState)EditorNativeCalls.GetPlayState();

    public static bool IsPlaying => State != PlayState.Edit;

    public static string ProjectRoot => EditorNativeCalls.GetProjectRoot();

    public static bool Execute(string name, string payload = "")
        => EditorNativeCalls.ExecuteCommand(name, payload);

    public static bool Undo() => EditorNativeCalls.Undo();

    public static bool Redo() => EditorNativeCalls.Redo();

    public static ulong CreateEntity(string name)
        => EditorNativeCalls.CreateEntity(name, out ulong uuid) ? uuid : 0;

    public static bool DeleteEntity(ulong uuid) => EditorNativeCalls.DeleteEntity(uuid);

    public static bool RenameEntity(ulong uuid, string name)
        => EditorNativeCalls.RenameEntity(uuid, name);

    public static bool ReparentEntity(ulong uuid, ulong parentUuid)
        => EditorNativeCalls.ReparentEntity(uuid, parentUuid);
}

public static class Selection
{
    public static ulong[] Get() => EditorNativeCalls.GetSelectionArray();

    public static ulong Active
    {
        get
        {
            ulong[] selection = EditorNativeCalls.GetSelectionArray();
            return selection.Length > 0 ? selection[0] : 0;
        }
    }

    public static bool IsEmpty => EditorNativeCalls.GetSelectionArray().Length == 0;

    public static bool Contains(ulong uuid)
    {
        foreach (ulong selected in EditorNativeCalls.GetSelectionArray())
        {
            if (selected == uuid)
                return true;
        }
        return false;
    }

    public static void Set(params ulong[] uuids) => EditorNativeCalls.SetSelection(uuids);

    public static void Set(ulong uuid) => EditorNativeCalls.SetSelection(new[] { uuid });

    public static void Clear() => EditorNativeCalls.SetSelection(Array.Empty<ulong>());
}

public static class EditorScene
{
    public static string Name => EditorNativeCalls.GetSceneName();

    public static int EntityCount => EditorNativeCalls.GetEntityCount();

    public static EntityInfo[] GetEntities()
    {
        int count = EditorNativeCalls.GetEntityCount();
        EntityInfo[] entities = new EntityInfo[count];
        for (int i = 0; i < count; ++i)
        {
            EditorNativeCalls.GetEntity(i, out ulong uuid, out ulong parent, out string name);
            entities[i] = new EntityInfo(uuid, parent, name);
        }
        return entities;
    }

    public static bool TryFind(string name, out EntityInfo entity)
    {
        foreach (EntityInfo candidate in GetEntities())
        {
            if (string.Equals(candidate.Name, name, StringComparison.Ordinal))
            {
                entity = candidate;
                return true;
            }
        }
        entity = default;
        return false;
    }
}
