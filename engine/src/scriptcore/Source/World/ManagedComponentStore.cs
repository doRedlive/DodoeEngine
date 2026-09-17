namespace GreenCake;

using System;
using System.Collections.Generic;

internal static class ManagedComponentStore
{
    private static readonly Dictionary<Type, ICakeComponentSet> _sets = new();

    private static class Handle<T> where T : CakeComponent
    {
        internal static readonly bool IsNative = ComponentManager.IsNative(typeof(T));
        private static ComponentSet<T>? _set;
        internal static ComponentSet<T> Set => _set ??= Register();

        private static ComponentSet<T> Register()
        {
            var set = new ComponentSet<T>();
            _sets[typeof(T)] = set;
            return set;
        }
    }

    public static void Add<T>(ulong entity, T component) where T : CakeComponent
    {
        Handle<T>.Set.Add(entity, component);
    }

    public static void AddOrReplace<T>(ulong entity, T component) where T : CakeComponent
    {
        Handle<T>.Set.AddOrReplace(entity, component);
    }

    public static bool Has<T>(ulong entity) where T : CakeComponent
    {
        if (Handle<T>.IsNative)
            return false;
        return Handle<T>.Set.Has(entity);
    }

    public static T Get<T>(ulong entity) where T : CakeComponent
    {
        return Handle<T>.IsNative ? null! : Handle<T>.Set.Get(entity);
    }

    public static bool TryGet<T>(ulong entity, out T component) where T : CakeComponent
    {
        if (Handle<T>.IsNative)
        {
            component = null!;
            return false;
        }
        return Handle<T>.Set.TryGet(entity, out component);
    }

    public static void Remove<T>(ulong entity) where T : CakeComponent
    {
        if (!Handle<T>.IsNative)
            Handle<T>.Set.Remove(entity);
    }

    public static void RemoveEntity(ulong entityId)
    {
        foreach (var set in _sets.Values)
            set.Remove(entityId);
    }

    public static IEnumerable<ulong> Query<T>() where T : CakeComponent
    {
        if (Handle<T>.IsNative)
            yield break;
        foreach (var entity in Handle<T>.Set.GetEntities())
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
        foreach (var set in _sets.Values)
            set.Clear();
    }
}
