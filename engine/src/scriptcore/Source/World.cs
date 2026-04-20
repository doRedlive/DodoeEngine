namespace GreenCake;

using System;
using System.Collections.Generic;

public class World
{
    internal static World Current { get; private set; }

    private readonly Dictionary<Type, IComponentSet> _componentSets = new();

    public World()
    {
        Current = this;
    }

    public Entity CreateEntity(string name = "Entity")
    {
        ulong entityId = InternalCalls.Native_CreateEntity(name);
        return new Entity(entityId);
    }

    public void DestroyEntity(Entity entity)
    {
        ulong entityId = entity.ID;
        InternalCalls.Native_DestroyEntity(entityId);

        foreach (var set in _componentSets.Values)
            set.Remove(entityId);
    }

    public void AddComponent<T>(ulong entity, T component) where T : Component
    {
        GetSet<T>().Add(entity, component);
    }

    public void AddOrReplaceComponent<T>(ulong entity, T component) where T : Component
    {
        GetSet<T>().AddOrReplace(entity, component);
    }

    public bool HasComponent<T>(ulong entity) where T : Component
    {
        return GetSet<T>().Has(entity);
    }

    public T GetComponent<T>(ulong entity) where T : Component
    {
        return GetSet<T>().Get(entity);
    }

    public bool TryGetComponent<T>(ulong entity, out T component) where T : Component
    {
        return GetSet<T>().TryGet(entity, out component);
    }

    public void RemoveComponent<T>(ulong entity) where T : Component
    {
        GetSet<T>().Remove(entity);
    }

    public IEnumerable<ulong> Query<T>() where T : Component
    {
        if (!TryGetSet<T>(out var set))
            yield break;

        foreach (var entity in set.GetEntities())
            yield return entity;
    }

    public IEnumerable<ulong> Query<T1, T2>()
        where T1 : Component
        where T2 : Component
    {
        foreach (var entity in Query(typeof(T1), typeof(T2)))
            yield return entity;
    }

    public IEnumerable<ulong> Query<T1, T2, T3>()
        where T1 : Component
        where T2 : Component
        where T3 : Component
    {
        foreach (var entity in Query(typeof(T1), typeof(T2), typeof(T3)))
            yield return entity;
    }

    public IEnumerable<ulong> Query<T1, T2, T3, T4>()
        where T1 : Component
        where T2 : Component
        where T3 : Component
        where T4 : Component
    {
        foreach (var entity in Query(typeof(T1), typeof(T2), typeof(T3), typeof(T4)))
            yield return entity;
    }

    public IEnumerable<ulong> Query<T1, T2, T3, T4, T5>()
        where T1 : Component
        where T2 : Component
        where T3 : Component
        where T4 : Component
        where T5 : Component
    {
        foreach (var entity in Query(typeof(T1), typeof(T2), typeof(T3), typeof(T4), typeof(T5)))
            yield return entity;
    }

    public IEnumerable<ulong> Query<T1, T2, T3, T4, T5, T6>()
        where T1 : Component
        where T2 : Component
        where T3 : Component
        where T4 : Component
        where T5 : Component
        where T6 : Component
    {
        foreach (var entity in Query(typeof(T1), typeof(T2), typeof(T3), typeof(T4), typeof(T5), typeof(T6)))
            yield return entity;
    }

    public IEnumerable<ulong> Query<T1, T2, T3, T4, T5, T6, T7>()
        where T1 : Component
        where T2 : Component
        where T3 : Component
        where T4 : Component
        where T5 : Component
        where T6 : Component
        where T7 : Component
    {
        foreach (var entity in Query(typeof(T1), typeof(T2), typeof(T3), typeof(T4), typeof(T5), typeof(T6), typeof(T7)))
            yield return entity;
    }

    public IEnumerable<ulong> Query<T1, T2, T3, T4, T5, T6, T7, T8>()
        where T1 : Component
        where T2 : Component
        where T3 : Component
        where T4 : Component
        where T5 : Component
        where T6 : Component
        where T7 : Component
        where T8 : Component
    {
        foreach (var entity in Query(typeof(T1), typeof(T2), typeof(T3), typeof(T4), typeof(T5), typeof(T6), typeof(T7), typeof(T8)))
            yield return entity;
    }

    public IEnumerable<ulong> Query(params Type[] componentTypes)
    {
        if (componentTypes is null)
            throw new ArgumentNullException(nameof(componentTypes));

        if (componentTypes.Length == 0)
            yield break;

        var sets = new List<IComponentSet>(componentTypes.Length);
        var uniqueTypes = new HashSet<Type>();

        foreach (var componentType in componentTypes)
        {
            if (componentType is null)
                throw new ArgumentException("Component type cannot be null.", nameof(componentTypes));

            if (!typeof(Component).IsAssignableFrom(componentType))
                throw new ArgumentException($"{componentType.FullName} does not inherit Component.", nameof(componentTypes));

            if (!uniqueTypes.Add(componentType))
                continue;

            if (!_componentSets.TryGetValue(componentType, out var set))
                yield break;

            sets.Add(set);
        }

        if (sets.Count == 0)
            yield break;

        IComponentSet smallest = sets[0];
        for (int i = 1; i < sets.Count; i++)
        {
            if (sets[i].Count < smallest.Count)
                smallest = sets[i];
        }

        foreach (var entity in smallest.GetEntities())
        {
            bool matchesAll = true;
            for (int i = 0; i < sets.Count; i++)
            {
                if (ReferenceEquals(sets[i], smallest))
                    continue;

                if (!sets[i].Has(entity))
                {
                    matchesAll = false;
                    break;
                }
            }

            if (matchesAll)
                yield return entity;
        }
    }

    private ComponentSet<T> GetSet<T>() where T : Component
    {
        var type = typeof(T);
        if (!_componentSets.ContainsKey(type))
            _componentSets[type] = new ComponentSet<T>();
        return (ComponentSet<T>)_componentSets[type];
    }

    private bool TryGetSet<T>(out ComponentSet<T> set) where T : Component
    {
        if (_componentSets.TryGetValue(typeof(T), out var rawSet))
        {
            set = (ComponentSet<T>)rawSet;
            return true;
        }

        set = default!;
        return false;
    }
}
