using System;
using System.Collections.Generic;

namespace GreenCake;

public class GameObject
{
    internal Entity Entity { get; private set; }

    private bool _activeSelf = true;
    private readonly List<Component> _userComponents = new List<Component>();

    public string Name
    {
        get
        {
            if (Entity == null) return "";
            return Entity.GetComponent<IDComponent>().Name;
        }
        set
        {
            if (Entity != null)
                Entity.GetComponent<IDComponent>().Name = value;
        }
    }

    public ulong ID { get { return Entity != null ? Entity.ID : 0; } }

    public Transform Transform { get; private set; }

    public bool ActiveSelf
    {
        get { return _activeSelf; }
        set
        {
            if (_activeSelf == value) return;
            _activeSelf = value;
            NotifyActiveStateChanged();
        }
    }

    public bool ActiveInHierarchy
    {
        get
        {
            if (!_activeSelf) return false;
            var p = Parent;
            return p == null || p.ActiveInHierarchy;
        }
    }

    public GameObject Parent
    {
        get
        {
            var parentTf = Transform.Parent;
            return parentTf != null ? parentTf.GameObject : null;
        }
        set
        {
            if (value == this)
                throw new InvalidOperationException("Cannot set parent to self.");
            Transform.Parent = value != null ? value.Transform : null;
        }
    }

    private GameObject() { }

    public T AddComponent<T>() where T : Component, new()
    {
        if (typeof(T) == typeof(Transform))
        {
            if (Transform != null)
                return (T)(object)Transform;
        }

        if (World.Current != null && World.Current.HasComponent<T>(ID))
            return World.Current.GetComponent<T>(ID);

        var component = new T();
        component.Entity = Entity;
        World.Current?.AddOrReplaceComponent(ID, component);

        Type componentType = typeof(T);
        if (InternalCalls.Native_ComponentExists(ID, componentType))
        {
            if (!InternalCalls.Native_EntityHasComponent(ID, componentType))
                InternalCalls.Native_EntityAddComponent(ID, component);
        }

        if (component is MonoBehaviour mb)
        {
            mb.GameObject = this;
            _userComponents.Add(mb);
            GameObjectManager.QueueAwake(mb);
        }
        else if (component is Transform tf)
        {
            tf.GameObject = this;
        }

        return component;
    }

    public T GetComponent<T>() where T : Component
    {
        if (World.Current == null) return null;
        if (World.Current.TryGetComponent<T>(ID, out var component))
            return component;
        return null;
    }

    public bool TryGetComponent<T>(out T component) where T : Component
    {
        component = GetComponent<T>();
        return component != null;
    }

    public bool HasComponent<T>() where T : Component
    {
        return World.Current != null && World.Current.HasComponent<T>(ID);
    }

    public void RemoveComponent<T>() where T : Component
    {
        if (World.Current == null) return;

        if (typeof(T) == typeof(Transform))
            return;

        if (World.Current.TryGetComponent<T>(ID, out var component))
        {
            if (component is MonoBehaviour mb)
            {
                if (!mb._destroyed)
                {
                    mb._destroyed = true;
                    try { mb.OnDestroy(); }
                    catch (Exception e) { Debug.LogError(string.Format("OnDestroy error: {0}", e)); }
                }
                _userComponents.Remove(mb);
            }
        }

        World.Current.RemoveComponent<T>(ID);

        Type componentType = typeof(T);
        if (InternalCalls.Native_ComponentExists(ID, componentType))
        {
            if (InternalCalls.Native_EntityHasComponent(ID, componentType))
                InternalCalls.Native_EntityRemoveComponent(ID, componentType);
        }
    }

    internal IEnumerable<MonoBehaviour> GetMonoBehaviours()
    {
        foreach (var comp in _userComponents)
        {
            if (comp is MonoBehaviour mb)
                yield return mb;
        }
    }

    public Entity GetEntity() { return Entity; }

    public static GameObject Create(string name = "GameObject")
    {
        if (World.Current == null)
            throw new InvalidOperationException("World not initialized. Cannot create GameObject before engine starts.");

        var entity = World.Current.CreateEntity(name);

        var go = new GameObject { Entity = entity };

        var tf = new Transform { Entity = entity, GameObject = go };
        World.Current.AddOrReplaceComponent(entity.ID, tf);
        go.Transform = tf;

        GameObjectManager.Register(go);

        return go;
    }

    public static void Destroy(GameObject obj)
    {
        if (obj == null) return;
        GameObjectManager.QueueDestroy(obj);
    }

    public static GameObject Find(string name)
    {
        return GameObjectManager.Find(name);
    }

    public static GameObject FindByID(ulong id)
    {
        return GameObjectManager.FindByID(id);
    }

    private void NotifyActiveStateChanged()
    {
        foreach (var comp in _userComponents)
        {
            if (comp is MonoBehaviour mb && !mb._destroyed)
            {
                if (_activeSelf && mb.Enabled)
                    try { mb.OnEnable(); } catch (Exception e) { Debug.LogError(string.Format("OnEnable error: {0}", e)); }
                else if (!_activeSelf)
                    try { mb.OnDisable(); } catch (Exception e) { Debug.LogError(string.Format("OnDisable error: {0}", e)); }
            }
        }
    }

    public override string ToString()
    {
        return string.Format("GameObject({0}, ID={1})", Name, ID);
    }
}
