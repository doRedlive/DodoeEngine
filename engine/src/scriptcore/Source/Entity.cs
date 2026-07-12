namespace GreenCake;

using System;

public class Entity
{
    public readonly ulong ID;

    protected Entity() { ID = 0; }
    internal Entity(ulong id) { ID = id; }

    public bool HasComponent<T>() where T : CakeComponent
    {
        return ComponentManager.Has<T>(this);
    }

    public T GetComponent<T>() where T : CakeComponent
    {
        return ComponentManager.Get<T>(this);
    }

    public void AddComponent<T>(T component) where T : CakeComponent, new()
    {
        ComponentManager.Add<T>(this);
    }

    public void AddComponent<T>() where T : CakeComponent, new()
    {
        ComponentManager.Add<T>(this);
    }

    public void RemoveComponent<T>() where T : CakeComponent
    {
        ComponentManager.Remove<T>(this);
    }
}
