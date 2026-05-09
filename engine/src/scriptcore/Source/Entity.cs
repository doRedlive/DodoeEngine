namespace GreenCake;

using System;
using System.Collections.Generic;

public class Entity
{
    public readonly ulong ID;

    protected Entity() { ID = 0; }
    internal Entity(ulong id) { ID = id; }

    public bool HasComponent<T>() where T : Component
    {
        if (World.Current?.HasComponent<T>(ID) == true)
            return true;
        return false;
    }

    public T GetComponent<T>() where T : Component
    {
        if (World.Current != null && World.Current.TryGetComponent<T>(ID, out T component))
            return component;

        return null;
        Debug.Log($"Entity {ID} does not have component {typeof(T).FullName}.");
    }

    public void AddComponent<T>(T component) where T : Component
    {
        if (HasComponent<T>()) return;

        component.Entity = this;
        World.Current.AddComponent(ID, component);

        Type componentType = typeof(T);
        if (InternalCalls.Native_ComponentExists(ID, componentType))
        {
            if (InternalCalls.Native_EntityHasComponent(ID, componentType))
                return;
            InternalCalls.Native_EntityAddComponent(ID, component);
        }
    }

    public void AddComponent<T>() where T : Component, new()
    {
        AddComponent(new T());
    }

    public void RemoveComponent<T>() where T : Component
    {
        if (!HasComponent<T>()) return;

        World.Current.RemoveComponent<T>(ID);

        Type componentType = typeof(T);
        if (InternalCalls.Native_ComponentExists(ID, componentType))
        {
            if (!InternalCalls.Native_EntityHasComponent(ID, componentType))
                return;
            InternalCalls.Native_EntityRemoveComponent(ID, componentType);
        }
    }

    public Type[] GetAllMonoComponentTypes()
    {
        if (World.Current is null)
            return Array.Empty<Type>();

        var result = new List<Type>();
        foreach (var type in World.Current.GetMonoComponentTypes(ID))
            result.Add(type);

        return result.ToArray();
    }
}
