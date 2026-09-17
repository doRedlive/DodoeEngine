namespace GreenCake;

using System;
using System.Collections.Generic;

internal static class NativeProxyFactory
{
    private const int kMaxCachePerType = 4096;

    private sealed class Entry
    {
        internal Func<Entity, NativeComponent> Factory = null!;
        internal readonly Dictionary<ulong, NativeComponent> Cache = new();
    }

    private static readonly Dictionary<Type, Entry> _entries = new();

    public static void Register<T>(Func<Entity, T> factory) where T : NativeComponent
    {
        _entries[typeof(T)] = new Entry { Factory = e => factory(e) };
    }

    public static T Create<T>(Entity entity) where T : NativeComponent
    {
        return (T)Create(typeof(T), entity);
    }

    public static NativeComponent Create(Type type, Entity entity)
    {
        if (_entries.TryGetValue(type, out var entry))
            return GetOrCreate(entry, entity);

        var instance = (NativeComponent)Activator.CreateInstance(type);
        instance.Entity = entity;
        return instance;
    }

    internal static void RemoveEntity(ulong entityId)
    {
        foreach (var entry in _entries.Values)
            entry.Cache.Remove(entityId);
    }

    internal static void ClearCaches()
    {
        foreach (var entry in _entries.Values)
            entry.Cache.Clear();
    }

    internal static bool IsRegistered<T>() where T : NativeComponent
    {
        return _entries.ContainsKey(typeof(T));
    }

    private static NativeComponent GetOrCreate(Entry entry, Entity entity)
    {
        if (entry.Cache.TryGetValue(entity.ID, out var cached))
        {
            cached.Entity = entity;
            return cached;
        }

        var created = entry.Factory(entity);
        created.Entity = entity;
        if (entry.Cache.Count >= kMaxCachePerType)
            entry.Cache.Clear();
        entry.Cache[entity.ID] = created;
        return created;
    }
}
