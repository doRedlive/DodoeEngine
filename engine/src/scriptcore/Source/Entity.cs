namespace GreenCake;

using System;

public class Entity
{
    protected Entity() { ID = 0; }
    internal Entity(ulong id) { ID = id; }

    public readonly ulong ID;

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

        throw new InvalidOperationException($"Entity {ID} does not have component {typeof(T).FullName}.");
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
}
