namespace GreenCake;

using System;
using System.Collections.Generic;

public class GameObject
{
    public Entity Entity { get; internal set; }
    internal Scene _scene;
    internal bool _destroyed;

    private bool _activeSelf = true;
    private readonly List<CakeComponent> _userComponents = new List<CakeComponent>();

    public string Name
    {
        get
        {
            if (Entity == null) return "";
            var id = Entity.GetComponent<TagComponent>();
            return id != null ? id.Tag : "";
        }
        set
        {
            if (Entity != null)
            {
                var id = Entity.GetComponent<TagComponent>();
                if (id != null) id.Tag = value ?? "";
            }
        }
    }

    public ulong ID => Entity != null ? Entity.ID : 0;

    public Scene Scene => _scene;

    public TransformComponent Transform
    {
        get
        {
            if (Entity == null) return null;
            return _transform ??= Entity.GetComponent<TransformComponent>();
        }
    }

    private TransformComponent _transform;

    public bool ActiveSelf
    {
        get => _activeSelf;
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
            if (_scene == null) return null;
            return _scene.GetParentGameObject(Entity.ID);
        }
        set
        {
            if (value == this)
                throw new InvalidOperationException("Cannot set parent to self.");
            if (_scene != null)
            {
                NativeCalls.Native_EntitySetParent(Entity.ID, value != null ? value.Entity.ID : 0);
                _scene.SetParent(Entity.ID, value);
            }
        }
    }

    public int ChildCount
    {
        get
        {
            if (_scene == null) return 0;
            return _scene.GetChildCount(Entity.ID);
        }
    }

    public GameObject GetChild(int index)
    {
        if (_scene == null) return null;
        return _scene.GetChild(Entity.ID, index);
    }

    internal GameObject() { }

    public T AddComponent<T>() where T : CakeComponent, new()
    {
        var component = ComponentManager.Add<T>(Entity);

        if (component is CakeBehaviour mb)
        {
            mb.GameObject = this;
            _userComponents.Add(mb);
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
        if (ComponentManager.TryGetUserComponent<T>(Entity, out var component) && component is CakeBehaviour mb)
            _userComponents.Remove(mb);

        ComponentManager.Remove<T>(Entity);
    }

    public T GetComponentInParent<T>() where T : CakeComponent
    {
        var cur = this;
        while (cur != null)
        {
            var c = cur.GetComponent<T>();
            if (c != null) return c;
            cur = cur.Parent;
        }
        return null;
    }

    public T[] GetComponentsInChildren<T>(bool includeInactive = false) where T : CakeComponent
    {
        var list = new List<T>();
        CollectComponents(this, list, includeInactive);
        return list.ToArray();
    }

    public T[] GetComponents<T>() where T : CakeComponent
    {
        var list = new List<T>();
        var direct = GetComponent<T>();
        if (direct != null) list.Add(direct);
        return list.ToArray();
    }

    private static void CollectComponents<T>(GameObject go, List<T> list, bool includeInactive) where T : CakeComponent
    {
        if (go == null) return;
        if (!includeInactive && !go.ActiveInHierarchy) return;
        var c = go.GetComponent<T>();
        if (c != null) list.Add(c);
        int n = go.ChildCount;
        for (int i = 0; i < n; i++)
        {
            CollectComponents(go.GetChild(i), list, includeInactive);
        }
    }

    internal IEnumerable<CakeBehaviour> GetBehaviours()
    {
        foreach (var comp in _userComponents)
        {
            if (comp is CakeBehaviour mb)
                yield return mb;
        }
    }

    public Entity GetEntity() => Entity;

    public static GameObject Create(string name = "GameObject")
    {
        var scene = SceneManager.ActiveScene;
        if (scene == null)
            throw new InvalidOperationException("No active scene. Load a scene before creating GameObjects.");
        return scene.CreateGameObject(name);
    }

    public static void Destroy(GameObject obj)
    {
        if (obj == null) return;
        obj._scene?.DestroyGameObject(obj);
    }

    public static GameObject Find(string name)
    {
        var scene = SceneManager.ActiveScene;
        return scene != null ? scene.Find(name) : null;
    }

    public static GameObject FindByID(ulong id)
    {
        var scene = SceneManager.ActiveScene;
        return scene != null ? scene.FindByID(id) : null;
    }

    private void NotifyActiveStateChanged()
    {
        foreach (var comp in _userComponents)
        {
            if (comp is CakeBehaviour mb && !mb._destroyed)
            {
                if (_activeSelf && mb.Enabled)
                    try { mb.OnEnable(); } catch (Exception e) { Debug.LogError($"OnEnable error: {e}"); }
                else if (!_activeSelf)
                    try { mb.OnDisable(); } catch (Exception e) { Debug.LogError($"OnDisable error: {e}"); }
            }
        }
    }

    public override string ToString()
    {
        return $"GameObject({Name}, ID={ID})";
    }
}
