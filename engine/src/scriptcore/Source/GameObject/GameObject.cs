using System;
using System.Collections.Generic;

namespace GreenCake;

public class GameObject
{
    internal Entity Entity { get; private set; }

    private bool _activeSelf = true;
    private readonly List<CakeComponent> _userComponents = new List<CakeComponent>();

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

    public T AddComponent<T>() where T : CakeComponent, new()
    {
        if (typeof(T) == typeof(Transform))
        {
            if (Transform != null)
                return (T)(object)Transform;
        }

        var component = ComponentManager.Add<T>(Entity);

        if (component is CakeBehaviour mb)
        {
            mb.GameObject = this;
            _userComponents.Add(mb);
        }
        else if (component is Transform tf)
        {
            tf.GameObject = this;
        }

        return component;
    }

    public T GetComponent<T>() where T : CakeComponent
    {
        return ComponentManager.Get<T>(Entity);
    }

    public bool TryGetComponent<T>(out T component) where T : CakeComponent
    {
        component = GetComponent<T>();
        return component != null;
    }

    public bool HasComponent<T>() where T : CakeComponent
    {
        return ComponentManager.Has<T>(Entity);
    }

    public void RemoveComponent<T>() where T : CakeComponent
    {
        if (typeof(T) == typeof(Transform))
            return;

        if (ComponentManager.TryGetUserComponent<T>(Entity, out var component) && component is CakeBehaviour mb)
        {
            _userComponents.Remove(mb);
        }

        ComponentManager.Remove<T>(Entity);
    }

    internal IEnumerable<CakeBehaviour> GetBehaviours()
    {
        foreach (var comp in _userComponents)
        {
            if (comp is CakeBehaviour mb)
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
            if (comp is CakeBehaviour mb && !mb._destroyed)
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
