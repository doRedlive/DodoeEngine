using System;
using System.Collections.Generic;

namespace GreenCake;

internal static class GameObjectManager
{
    private static readonly Dictionary<ulong, GameObject> _objects = new Dictionary<ulong, GameObject>();

    private static readonly Dictionary<ulong, ulong> _parentMap = new Dictionary<ulong, ulong>();
    private static readonly Dictionary<ulong, List<ulong>> _childrenMap = new Dictionary<ulong, List<ulong>>();

    private static readonly List<MonoBehaviour> _awakeQueue = new List<MonoBehaviour>();
    private static readonly List<MonoBehaviour> _startQueue = new List<MonoBehaviour>();
    private static readonly List<MonoBehaviour> _activeBehaviours = new List<MonoBehaviour>();
    private static readonly List<GameObject> _destroyQueue = new List<GameObject>();

    public static void Register(GameObject obj)
    {
        if (obj == null) return;
        _objects[obj.ID] = obj;
    }

    public static void Unregister(ulong id)
    {
        _objects.Remove(id);

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

    public static void SetParent(ulong childId, GameObject parent)
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

    public static Transform GetParentTransform(ulong entityId)
    {
        if (_parentMap.TryGetValue(entityId, out ulong parentId))
        {
            if (_objects.TryGetValue(parentId, out var parentGo))
                return parentGo.Transform;
        }
        return null;
    }

    public static int GetChildCount(ulong entityId)
    {
        if (_childrenMap.TryGetValue(entityId, out var children))
            return children.Count;
        return 0;
    }

    public static Transform GetChild(ulong entityId, int index)
    {
        if (_childrenMap.TryGetValue(entityId, out var children))
        {
            if (index >= 0 && index < children.Count)
            {
                if (_objects.TryGetValue(children[index], out var childGo))
                    return childGo.Transform;
            }
        }
        return null;
    }

    public static GameObject FindByID(ulong id)
    {
        _objects.TryGetValue(id, out var obj);
        return obj;
    }

    public static GameObject Find(string name)
    {
        foreach (var obj in _objects.Values)
        {
            if (obj.Name == name)
                return obj;
        }
        return null;
    }

    public static void QueueAwake(MonoBehaviour comp)
    {
        if (!comp._awakeCalled && !comp._destroyed)
            _awakeQueue.Add(comp);
    }

    public static void QueueStart(MonoBehaviour comp)
    {
        if (!comp._startCalled && !comp._destroyed)
            _startQueue.Add(comp);
    }

    public static void QueueDestroy(GameObject obj)
    {
        if (!_destroyQueue.Contains(obj))
            _destroyQueue.Add(obj);
    }

    public static void RegisterActiveBehaviour(MonoBehaviour comp)
    {
        if (!_activeBehaviours.Contains(comp))
            _activeBehaviours.Add(comp);
    }

    public static void ProcessLifecycle(float dt)
    {
        ProcessAwakeQueue();
        ProcessStartQueue();
        ProcessUpdates(dt);
        ProcessDestroyQueue();
    }

    private static void ProcessAwakeQueue()
    {
        if (_awakeQueue.Count == 0) return;

        var toProcess = new List<MonoBehaviour>(_awakeQueue);
        _awakeQueue.Clear();

        foreach (var comp in toProcess)
        {
            if (comp._destroyed || comp._awakeCalled) continue;
            comp._awakeCalled = true;

            if (comp.GameObject != null && comp.GameObject.ActiveInHierarchy && comp.Enabled)
            {
                try { comp.Awake(); }
                catch (Exception e) { Debug.LogError(string.Format("Awake error in {0}: {1}", comp.GetType().Name, e)); }
            }

            QueueStart(comp);
        }
    }

    private static void ProcessStartQueue()
    {
        if (_startQueue.Count == 0) return;

        var toProcess = new List<MonoBehaviour>(_startQueue);
        _startQueue.Clear();

        foreach (var comp in toProcess)
        {
            if (comp._destroyed || comp._startCalled) continue;
            comp._startCalled = true;

            if (comp.GameObject != null && comp.GameObject.ActiveInHierarchy && comp.Enabled)
            {
                try { comp.Start(); }
                catch (Exception e) { Debug.LogError(string.Format("Start error in {0}: {1}", comp.GetType().Name, e)); }
            }

            RegisterActiveBehaviour(comp);
        }
    }

    private static void ProcessUpdates(float dt)
    {
        foreach (var comp in _activeBehaviours)
        {
            if (comp._destroyed) continue;
            if (!comp.Enabled) continue;
            if (comp.GameObject == null) continue;
            if (!comp.GameObject.ActiveInHierarchy) continue;

            try { comp.Update(); }
            catch (Exception e) { Debug.LogError(string.Format("Update error in {0}: {1}", comp.GetType().Name, e)); }
        }
    }

    private static void ProcessDestroyQueue()
    {
        if (_destroyQueue.Count == 0) return;

        var toDestroy = new List<GameObject>(_destroyQueue);
        _destroyQueue.Clear();

        foreach (var go in toDestroy)
        {
            if (go == null) continue;

            if (_childrenMap.TryGetValue(go.ID, out var children))
            {
                var childrenCopy = new List<ulong>(children);
                foreach (var childId in childrenCopy)
                {
                    if (_objects.TryGetValue(childId, out var childGo))
                        QueueDestroy(childGo);
                }
            }

            foreach (var comp in _activeBehaviours)
            {
                if (comp.GameObject == go && !comp._destroyed)
                {
                    comp._destroyed = true;
                    try { comp.OnDestroy(); }
                    catch (Exception e) { Debug.LogError(string.Format("OnDestroy error in {0}: {1}", comp.GetType().Name, e)); }
                }
            }

            _activeBehaviours.RemoveAll(mb => mb.GameObject == go);

            InternalCalls.Native_DestroyEntity(go.ID);

            Unregister(go.ID);
        }
    }
}
