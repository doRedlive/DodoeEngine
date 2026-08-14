namespace GreenCake;

using System;
using System.Collections.Generic;
using System.Text.Json;

public class Scene
{
    internal string _name;
    internal bool _isLoaded;
    internal bool _isStarted;

    private readonly Dictionary<ulong, GameObject> _gameObjects = new();
    private readonly Dictionary<ulong, ulong> _parentMap = new();
    private readonly Dictionary<ulong, List<ulong>> _childrenMap = new();

    private readonly List<CakeBehaviour> _awakeQueue = new();
    private readonly List<CakeBehaviour> _startQueue = new();
    private readonly List<CakeBehaviour> _activeBehaviours = new();
    private readonly List<GameObject> _destroyQueue = new();

    public string Name => _name;
    public bool IsLoaded => _isLoaded;
    public bool IsStarted => _isStarted;

    public event Action<Scene> OnSceneLoad;
    public event Action<Scene> OnSceneStart;
    public event Action<Scene, float> OnSceneUpdate;
    public event Action<Scene> OnSceneStop;
    public event Action<Scene> OnSceneUnload;

    internal Scene(string name)
    {
        _name = name;
    }

    public GameObject CreateGameObject(string name = "GameObject")
    {
        if (World.Current == null)
            throw new InvalidOperationException("World not initialized.");

        var entity = World.Current.CreateEntity(name);
        var go = new GameObject { Entity = entity, _scene = this };

        _gameObjects[go.ID] = go;
        return go;
    }

    internal GameObject RegisterEntity(ulong id)
    {
        if (_gameObjects.TryGetValue(id, out var existing))
            return existing;

        var entity = new Entity(id);
        var go = new GameObject { Entity = entity, _scene = this };

        _gameObjects[go.ID] = go;
        return go;
    }

    internal void SyncFromNative()
    {
        var json = NativeCalls.Native_WorldGetActiveSceneEntities();
        if (string.IsNullOrEmpty(json))
            return;
        try
        {
            using var doc = JsonDocument.Parse(json);
            foreach (var item in doc.RootElement.EnumerateArray())
            {
                if (!item.TryGetProperty("id", out var idProp) || idProp.ValueKind != JsonValueKind.Number)
                    continue;
                RegisterEntity(idProp.GetUInt64());
            }
        }
        catch (JsonException) { }
    }

    public void DestroyGameObject(GameObject obj)
    {
        if (obj == null || obj._destroyed) return;
        if (!_destroyQueue.Contains(obj))
            _destroyQueue.Add(obj);
    }

    public GameObject Find(string name)
    {
        foreach (var go in _gameObjects.Values)
        {
            if (go.Name == name)
                return go;
        }
        return null;
    }

    public GameObject FindByID(ulong id)
    {
        _gameObjects.TryGetValue(id, out var go);
        return go;
    }

    public GameObject FindByTag(string tag)
    {
        foreach (var entity in World.Current.Query<TagComponent>())
        {
            var tagComp = entity.GetComponent<TagComponent>();
            if (tagComp != null && tagComp.Tag == tag)
            {
                _gameObjects.TryGetValue(entity.ID, out var go);
                if (go != null) return go;
            }
        }
        return null;
    }

    public List<GameObject> FindAllByTag(string tag)
    {
        var result = new List<GameObject>();
        foreach (var entity in World.Current.Query<TagComponent>())
        {
            var tagComp = entity.GetComponent<TagComponent>();
            if (tagComp != null && tagComp.Tag == tag)
            {
                _gameObjects.TryGetValue(entity.ID, out var go);
                if (go != null) result.Add(go);
            }
        }
        return result;
    }

    public List<GameObject> GetAllGameObjects()
    {
        return new List<GameObject>(_gameObjects.Values);
    }

    internal void QueueAwake(CakeBehaviour comp)
    {
        if (!comp._awakeCalled && !comp._destroyed)
            _awakeQueue.Add(comp);
    }

    internal void QueueStart(CakeBehaviour comp)
    {
        if (!comp._startCalled && !comp._destroyed)
            _startQueue.Add(comp);
    }

    internal void RegisterActiveBehaviour(CakeBehaviour comp)
    {
        if (!_activeBehaviours.Contains(comp))
            _activeBehaviours.Add(comp);
    }

    internal void Unregister(ulong id)
    {
        _gameObjects.Remove(id);

        if (_parentMap.TryGetValue(id, out ulong parentId))
        {
            _parentMap.Remove(id);
            if (_childrenMap.TryGetValue(parentId, out var siblings))
                siblings.Remove(id);
        }

        if (_childrenMap.TryGetValue(id, out var children))
        {
            foreach (var childId in children)
                _parentMap.Remove(childId);
            _childrenMap.Remove(id);
        }

        _activeBehaviours.RemoveAll(mb => mb.GameObject != null && mb.GameObject.ID == id);
    }

    internal void SetParent(ulong childId, GameObject parent)
    {
        if (_parentMap.TryGetValue(childId, out ulong oldParentId))
        {
            _parentMap.Remove(childId);
            if (_childrenMap.TryGetValue(oldParentId, out var oldSiblings))
                oldSiblings.Remove(childId);
        }

        if (parent != null)
        {
            _parentMap[childId] = parent.ID;
            if (!_childrenMap.ContainsKey(parent.ID))
                _childrenMap[parent.ID] = new List<ulong>();
            _childrenMap[parent.ID].Add(childId);
        }
    }

    internal GameObject GetParentGameObject(ulong entityId)
    {
        if (_parentMap.TryGetValue(entityId, out ulong parentId))
        {
            if (_gameObjects.TryGetValue(parentId, out var parentGo))
                return parentGo;
        }
        return null;
    }

    internal int GetChildCount(ulong entityId)
    {
        if (_childrenMap.TryGetValue(entityId, out var children))
            return children.Count;
        return 0;
    }

    internal GameObject GetChild(ulong entityId, int index)
    {
        if (_childrenMap.TryGetValue(entityId, out var children))
        {
            if (index >= 0 && index < children.Count)
            {
                if (_gameObjects.TryGetValue(children[index], out var childGo))
                    return childGo;
            }
        }
        return null;
    }

    private void ProcessLifecycle(float dt)
    {
        ProcessAwakeQueue();
        ProcessStartQueue();
        ProcessUpdates(dt);
        ProcessDestroyQueue();
    }

    private void ProcessAwakeQueue()
    {
        if (_awakeQueue.Count == 0) return;
        var toProcess = new List<CakeBehaviour>(_awakeQueue);
        _awakeQueue.Clear();

        foreach (var comp in toProcess)
        {
            if (comp._destroyed || comp._awakeCalled) continue;
            comp._awakeCalled = true;

            if (comp.GameObject != null && comp.GameObject.ActiveInHierarchy && comp.Enabled)
            {
                try { comp.Awake(); }
                catch (Exception e) { Debug.LogError($"Awake error in {comp.GetType().Name}: {e}"); }
            }

            QueueStart(comp);
        }
    }

    private void ProcessStartQueue()
    {
        if (_startQueue.Count == 0) return;
        var toProcess = new List<CakeBehaviour>(_startQueue);
        _startQueue.Clear();

        foreach (var comp in toProcess)
        {
            if (comp._destroyed || comp._startCalled) continue;
            comp._startCalled = true;

            if (comp.GameObject != null && comp.GameObject.ActiveInHierarchy && comp.Enabled)
            {
                try { comp.Start(); }
                catch (Exception e) { Debug.LogError($"Start error in {comp.GetType().Name}: {e}"); }
            }

            RegisterActiveBehaviour(comp);
        }
    }

    private void ProcessUpdates(float dt)
    {
        foreach (var comp in _activeBehaviours)
        {
            if (comp._destroyed) continue;
            if (!comp.Enabled) continue;
            if (comp.GameObject == null) continue;
            if (!comp.GameObject.ActiveInHierarchy) continue;

            try { comp.Update(); }
            catch (Exception e) { Debug.LogError($"Update error in {comp.GetType().Name}: {e}"); }
        }
    }

    private void ProcessDestroyQueue()
    {
        if (_destroyQueue.Count == 0) return;
        var toDestroy = new List<GameObject>(_destroyQueue);
        _destroyQueue.Clear();

        foreach (var go in toDestroy)
        {
            if (go == null || go._destroyed) continue;
            go._destroyed = true;

            if (_childrenMap.TryGetValue(go.ID, out var children))
            {
                var childrenCopy = new List<ulong>(children);
                foreach (var childId in childrenCopy)
                {
                    if (_gameObjects.TryGetValue(childId, out var childGo))
                        DestroyGameObject(childGo);
                }
            }

            foreach (var comp in _activeBehaviours)
            {
                if (comp.GameObject == go && !comp._destroyed)
                {
                    comp._destroyed = true;
                    try { comp.OnDestroy(); }
                    catch (Exception e) { Debug.LogError($"OnDestroy error in {comp.GetType().Name}: {e}"); }
                }
            }

            _activeBehaviours.RemoveAll(mb => mb.GameObject == go);
            World.Current.CommandBuffer.DestroyEntity(go.ID);
            Unregister(go.ID);
        }
    }

    internal void NotifyLoad()
    {
        _isLoaded = true;
        try { OnSceneLoad?.Invoke(this); }
        catch (Exception e) { Debug.LogError($"OnSceneLoad error: {e}"); }
    }

    internal void NotifyStart()
    {
        _isStarted = true;
        try { OnSceneStart?.Invoke(this); }
        catch (Exception e) { Debug.LogError($"OnSceneStart error: {e}"); }
    }

    internal void NotifyUpdate(float deltaTime)
    {
        ProcessLifecycle(deltaTime);
        try { OnSceneUpdate?.Invoke(this, deltaTime); }
        catch (Exception e) { Debug.LogError($"OnSceneUpdate error: {e}"); }
    }

    internal void NotifyStop()
    {
        _isStarted = false;
        try { OnSceneStop?.Invoke(this); }
        catch (Exception e) { Debug.LogError($"OnSceneStop error: {e}"); }
    }

    internal void NotifyUnload()
    {
        _isLoaded = false;
        _isStarted = false;

        var allObjects = new List<GameObject>(_gameObjects.Values);
        foreach (var go in allObjects)
        {
            if (!go._destroyed)
            {
                go._destroyed = true;
                foreach (var comp in _activeBehaviours)
                {
                    if (comp.GameObject == go && !comp._destroyed)
                    {
                        comp._destroyed = true;
                        try { comp.OnDestroy(); }
                        catch (Exception e) { Debug.LogError($"OnDestroy error in {comp.GetType().Name}: {e}"); }
                    }
                }
                NativeCalls.Native_DestroyEntity(go.ID);
            }
        }

        _gameObjects.Clear();
        _parentMap.Clear();
        _childrenMap.Clear();
        _awakeQueue.Clear();
        _startQueue.Clear();
        _activeBehaviours.Clear();
        _destroyQueue.Clear();

        try { OnSceneUnload?.Invoke(this); }
        catch (Exception e) { Debug.LogError($"OnSceneUnload error: {e}"); }

        OnSceneLoad = null;
        OnSceneStart = null;
        OnSceneUpdate = null;
        OnSceneStop = null;
        OnSceneUnload = null;
    }

    public override string ToString()
    {
        return $"Scene({_name}, Loaded={_isLoaded}, Started={_isStarted}, Objects={_gameObjects.Count})";
    }
}
