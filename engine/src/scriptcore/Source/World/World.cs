namespace GreenCake;

using System;
using System.Collections.Generic;

public class World
{
    internal static World Current
    {
        get
        {
            _current ??= new World();
            return _current;
        }
    }

    private static World? _current;

    public CakeCommandBuffer CommandBuffer { get; } = new();

    private World()
    {
        _current = this;
    }

    public Entity CreateEntity(string name = "Entity")
    {
        ulong entityId = NativeCalls.Native_CreateEntity(name);
        return new Entity(entityId);
    }

    public void DestroyEntity(Entity entity)
    {
        ulong entityId = entity.ID;
        NativeCalls.Native_DestroyEntity(entityId);
        RemoveEntityLocal(entityId);
    }

    public void AddOrReplaceComponent<T>(ulong entity, T component) where T : CakeComponent
    {
        ManagedComponentStore.AddOrReplace(entity, component);
    }

    public bool TryGetComponent<T>(ulong entity, out T component) where T : CakeComponent
    {
        return ManagedComponentStore.TryGet(entity, out component);
    }

    public bool TryGetComponent(ulong entity, Type componentType, out CakeComponent component)
    {
        return ManagedComponentStore.TryGetComponent(entity, componentType, out component);
    }

    public IEnumerable<Entity> Query<T>() where T : CakeComponent
    {
        foreach (var entityId in ManagedComponentStore.Query<T>())
            yield return new Entity(entityId);
    }

    public IEnumerable<Entity> Query<T1, T2>()
        where T1 : CakeComponent
        where T2 : CakeComponent
    {
        foreach (var entityId in ManagedComponentStore.Query(typeof(T1), typeof(T2)))
            yield return new Entity(entityId);
    }

    public IEnumerable<Entity> Query<T1, T2, T3>()
        where T1 : CakeComponent
        where T2 : CakeComponent
        where T3 : CakeComponent
    {
        foreach (var entityId in ManagedComponentStore.Query(typeof(T1), typeof(T2), typeof(T3)))
            yield return new Entity(entityId);
    }

    public IEnumerable<Entity> Query<T1, T2, T3, T4>()
        where T1 : CakeComponent
        where T2 : CakeComponent
        where T3 : CakeComponent
        where T4 : CakeComponent
    {
        foreach (var entityId in ManagedComponentStore.Query(typeof(T1), typeof(T2), typeof(T3), typeof(T4)))
            yield return new Entity(entityId);
    }

    public IEnumerable<Entity> Query<T1, T2, T3, T4, T5>()
        where T1 : CakeComponent
        where T2 : CakeComponent
        where T3 : CakeComponent
        where T4 : CakeComponent
        where T5 : CakeComponent
    {
        foreach (var entityId in ManagedComponentStore.Query(typeof(T1), typeof(T2), typeof(T3), typeof(T4), typeof(T5)))
            yield return new Entity(entityId);
    }

    public IEnumerable<Entity> Query(params Type[] componentTypes)
    {
        foreach (var entityId in ManagedComponentStore.Query(componentTypes))
            yield return new Entity(entityId);
    }

    internal IEnumerable<Type> GetManagedComponentTypes(ulong entity)
    {
        return ManagedComponentStore.GetComponentTypes(entity);
    }

    internal void RemoveEntityLocal(ulong entityId)
    {
        ManagedComponentStore.RemoveEntity(entityId);
    }

    internal static void Reset()
    {
        ManagedComponentStore.Clear();
        _current = null;
    }
}
