namespace GreenCake;

using System;
using System.Collections.Generic;

internal interface ICakeComponentSet
{
    int Count { get; }
    void Remove(ulong entityId);
    bool Has(ulong entityId);
    IEnumerable<ulong> GetEntities();
    bool TryGetComponent(ulong entityId, out CakeComponent component);
}

internal class ComponentSet<T> : ICakeComponentSet where T : CakeComponent
{
    private readonly Dictionary<ulong, int> _sparse = new();
    private ulong[] _entities = new ulong[0];
    private T[] _dense = new T[0];
    private int _count;
    private int _capacity;

    public int Count => _count;

    public ref struct Enumerator
    {
        private readonly T[] _dense;
        private readonly int _count;
        private int _index;

        public Enumerator(T[] dense, int count)
        {
            _dense = dense;
            _count = count;
            _index = -1;
        }

        public bool MoveNext()
        {
            while (++_index < _count)
            {
                if (_dense[_index] != null)
                    return true;
            }
            return false;
        }

        public T Current => _dense[_index];
        public Enumerator GetEnumerator() => this;
    }

    public Enumerator Query() => new(_dense, _count);

    public void Add(ulong entityId, T component)
    {
        if (Has(entityId))
            return;

        EnsureCapacity(_count + 1);

        int denseIndex = _count;
        _entities[denseIndex] = entityId;
        _dense[denseIndex] = component;
        _sparse[entityId] = denseIndex;

        _count++;
    }

    public void AddOrReplace(ulong entityId, T component)
    {
        if (_sparse.TryGetValue(entityId, out int index))
        {
            if (index >= 0 && index < _count && _entities[index] == entityId)
            {
                _dense[index] = component;
                return;
            }
            _sparse.Remove(entityId);
        }

        Add(entityId, component);
    }

    public void Remove(ulong entityId)
    {
        if (!_sparse.TryGetValue(entityId, out int removeIndex))
            return;
        if (removeIndex < 0 || removeIndex >= _count || _entities[removeIndex] != entityId)
        {
            _sparse.Remove(entityId);
            return;
        }

        int lastIndex = _count - 1;
        if (removeIndex != lastIndex)
        {
            ulong movedEntity = _entities[lastIndex];
            T movedComponent = _dense[lastIndex];

            _entities[removeIndex] = movedEntity;
            _dense[removeIndex] = movedComponent;
            _sparse[movedEntity] = removeIndex;
        }

        _entities[lastIndex] = default!;
        _dense[lastIndex] = null!;
        _sparse.Remove(entityId);
        _count--;
    }

    public bool Has(ulong entityId)
    {
        if (!_sparse.TryGetValue(entityId, out int index))
            return false;
        return index >= 0 && index < _count && _entities[index] == entityId;
    }

    public T Get(ulong entityId)
    {
        if (!_sparse.TryGetValue(entityId, out int index))
            return default!;
        if (index < 0 || index >= _count || _entities[index] != entityId)
            return default!;
        return _dense[index];
    }

    public bool TryGet(ulong entityId, out T component)
    {
        if (_sparse.TryGetValue(entityId, out int index) &&
            index >= 0 && index < _count && _entities[index] == entityId)
        {
            component = _dense[index];
            return true;
        }

        component = default!;
        return false;
    }

    public bool TryGetComponent(ulong entityId, out CakeComponent component)
    {
        if (TryGet(entityId, out T typedComponent) && typedComponent is not null)
        {
            component = typedComponent;
            return true;
        }

        component = null!;
        return false;
    }

    public IEnumerable<ulong> GetEntities()
    {
        for (int i = 0; i < _count; i++)
        {
            if (_dense[i] != null)
                yield return _entities[i];
        }
    }

    private void EnsureCapacity(int required)
    {
        if (required <= _capacity)
            return;

        int newCapacity = _capacity == 0 ? 16 : _capacity * 2;
        while (newCapacity < required)
            newCapacity *= 2;

        Array.Resize(ref _entities, newCapacity);
        Array.Resize(ref _dense, newCapacity);
        _capacity = newCapacity;
    }
}
