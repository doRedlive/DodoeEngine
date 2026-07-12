namespace GreenCake;

using System;
using System.Collections.Generic;

internal static class ManagedComponentStore
{
    private static readonly Dictionary<Type, ICakeComponentSet> _sets = new();

    public static void Add<T>(ulong entity, T component) where T : CakeComponent
    {
        GetSet<T>().Add(entity, component);
    }

    public static void AddOrReplace<T>(ulong entity, T component) where T : CakeComponent
    {
        GetSet<T>().AddOrReplace(entity, component);
    }

    public static bool Has<T>(ulong entity) where T : CakeComponent
    {
        if (ComponentManager.IsNative(typeof(T)))
            return false;
        return TryGetSet<T>(out var set) && set.Has(entity);
    }

    public static T Get<T>(ulong entity) where T : CakeComponent
    {
        return TryGetSet<T>(out var set) ? set.Get(entity) : null;
    }

    public static bool TryGet<T>(ulong entity, out T component) where T : CakeComponent
    {
        if (TryGetSet<T>(out var set))
            return set.TryGet(entity, out component);
        component = null;
        return false;
    }

    public static void Remove<T>(ulong entity) where T : CakeComponent
    {
        if (TryGetSet<T>(out var set))
            set.Remove(entity);
    }

    public static void RemoveEntity(ulong entityId)
    {
        foreach (var set in _sets.Values)
            set.Remove(entityId);
    }

    public static IEnumerable<ulong> Query<T>() where T : CakeComponent
    {
        if (!TryGetSet<T>(out var set))
            yield break;
        foreach (var entity in set.GetEntities())
            yield return entity;
    }

    public static IEnumerable<ulong> Query(params Type[] componentTypes)
    {
        if (componentTypes is null || componentTypes.Length == 0)
            yield break;

        var matchingSets = new List<ICakeComponentSet>();
        foreach (var t in componentTypes)
        {
            if (ComponentManager.IsNative(t))
                continue;
            if (_sets.TryGetValue(t, out var set))
                matchingSets.Add(set);
            else
                yield break;
        }

        if (matchingSets.Count == 0)
            yield break;

        var smallest = matchingSets[0];
        for (int i = 1; i < matchingSets.Count; i++)
            if (matchingSets[i].Count < smallest.Count)
                smallest = matchingSets[i];

        foreach (var entity in smallest.GetEntities())
        {
            bool matchesAll = true;
            foreach (var set in matchingSets)
            {
                if (ReferenceEquals(set, smallest))
                    continue;
                if (!set.Has(entity))
                {
                    matchesAll = false;
                    break;
                }
            }
            if (matchesAll)
                yield return entity;
        }
    }

    public static IEnumerable<Type> GetComponentTypes(ulong entity)
    {
        foreach (var (t, set) in _sets)
            if (set.Has(entity))
                yield return t;
    }

    public static IEnumerable<CakeComponent> GetAllComponents(ulong entity)
    {
        foreach (var (_, set) in _sets)
        {
            if (set.TryGetComponent(entity, out var comp) && comp is not null)
                yield return comp;
        }
    }

    public static bool TryGetComponent(ulong entity, Type componentType, out CakeComponent component)
    {
        if (_sets.TryGetValue(componentType, out var set) && set.TryGetComponent(entity, out component))
            return true;
        component = null!;
        return false;
    }

    public static void Clear()
    {
        _sets.Clear();
    }

    private static ComponentSet<T> GetSet<T>() where T : CakeComponent
    {
        var type = typeof(T);
        if (!_sets.ContainsKey(type))
            _sets[type] = new ComponentSet<T>();
        return (ComponentSet<T>)_sets[type];
    }

    private static bool TryGetSet<T>(out ComponentSet<T> set) where T : CakeComponent
    {
        if (ComponentManager.IsNative(typeof(T)))
        {
            set = default!;
            return false;
        }
        if (_sets.TryGetValue(typeof(T), out var rawSet))
        {
            set = (ComponentSet<T>)rawSet;
            return true;
        }
        set = default!;
        return false;
    }
}
