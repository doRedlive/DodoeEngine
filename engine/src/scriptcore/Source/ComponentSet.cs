namespace GreenCake;

using System.Collections.Generic;

internal interface IComponentSet
{
    int Count { get; }
    void Remove(ulong entityId);
    bool Has(ulong entityId);
    IEnumerable<ulong> GetEntities();
}

internal class ComponentSet<T> : IComponentSet where T : Component
{
    private readonly Dictionary<ulong, int> _sparse = new();
    private readonly List<ulong> _entities = new();
    private readonly List<T> _dense = new();

    public int Count => _dense.Count;

    public void Add(ulong entityId, T component)
    {
        if (_sparse.ContainsKey(entityId))
            return;

        _sparse[entityId] = _dense.Count;
        _entities.Add(entityId);
        _dense.Add(component);
    }

    public void AddOrReplace(ulong entityId, T component)
    {
        if (_sparse.TryGetValue(entityId, out int index))
        {
            _dense[index] = component;
            return;
        }

        Add(entityId, component);
    }

    public void Remove(ulong entityId)
    {
        if (!_sparse.TryGetValue(entityId, out int removeIndex))
            return;

        int lastIndex = _dense.Count - 1;
        if (removeIndex != lastIndex)
        {
            ulong movedEntity = _entities[lastIndex];
            T movedComponent = _dense[lastIndex];

            _entities[removeIndex] = movedEntity;
            _dense[removeIndex] = movedComponent;
            _sparse[movedEntity] = removeIndex;
        }

        _entities.RemoveAt(lastIndex);
        _dense.RemoveAt(lastIndex);
        _sparse.Remove(entityId);
    }

    public bool Has(ulong entityId)
    {
        return _sparse.ContainsKey(entityId);
    }

    public T Get(ulong entityId)
    {
        return _sparse.TryGetValue(entityId, out int index) ? _dense[index] : default;
    }

    public bool TryGet(ulong entityId, out T component)
    {
        if (_sparse.TryGetValue(entityId, out int index))
        {
            component = _dense[index];
            return true;
        }

        component = default;
        return false;
    }

    public IEnumerable<ulong> GetEntities()
    {
        foreach (var entityId in _entities)
            yield return entityId;
    }
}
