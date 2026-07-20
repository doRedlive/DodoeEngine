namespace GreenCake;

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
    private const int PageSize = 4096;
    private const int PageShift = 12;
    private const int PageMask = 0xFFF;

    private int[][] _sparse = new int[0][];
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

        int page = (int)(entityId >> PageShift);
        int offset = (int)(entityId & PageMask);
        EnsurePage(page);
        _sparse[page][offset] = denseIndex;

        _count++;
    }

    public void AddOrReplace(ulong entityId, T component)
    {
        int page = (int)(entityId >> PageShift);
        int offset = (int)(entityId & PageMask);
        if (page < _sparse.Length && _sparse[page] != null)
        {
            int index = _sparse[page][offset];
            if (index >= 0 && index < _count && _entities[index] == entityId)
            {
                _dense[index] = component;
                return;
            }
        }

        Add(entityId, component);
    }

    public void Remove(ulong entityId)
    {
        int page = (int)(entityId >> PageShift);
        int offset = (int)(entityId & PageMask);
        if (page >= _sparse.Length || _sparse[page] == null)
            return;

        int removeIndex = _sparse[page][offset];
        if (removeIndex < 0 || removeIndex >= _count || _entities[removeIndex] != entityId)
            return;

        int lastIndex = _count - 1;
        if (removeIndex != lastIndex)
        {
            ulong movedEntity = _entities[lastIndex];
            T movedComponent = _dense[lastIndex];

            _entities[removeIndex] = movedEntity;
            _dense[removeIndex] = movedComponent;

            int movedPage = (int)(movedEntity >> PageShift);
            int movedOffset = (int)(movedEntity & PageMask);
            _sparse[movedPage][movedOffset] = removeIndex;
        }

        _entities[lastIndex] = default!;
        _dense[lastIndex] = null!;
        _sparse[page][offset] = -1;
        _count--;
    }

    public bool Has(ulong entityId)
    {
        int page = (int)(entityId >> PageShift);
        int offset = (int)(entityId & PageMask);
        if (page >= _sparse.Length || _sparse[page] == null)
            return false;
        int index = _sparse[page][offset];
        return index >= 0 && index < _count && _entities[index] == entityId;
    }

    public T Get(ulong entityId)
    {
        int page = (int)(entityId >> PageShift);
        int offset = (int)(entityId & PageMask);
        if (page >= _sparse.Length || _sparse[page] == null)
            return default!;
        int index = _sparse[page][offset];
        if (index < 0 || index >= _count || _entities[index] != entityId)
            return default!;
        return _dense[index];
    }

    public bool TryGet(ulong entityId, out T component)
    {
        int page = (int)(entityId >> PageShift);
        int offset = (int)(entityId & PageMask);
        if (page < _sparse.Length && _sparse[page] != null)
        {
            int index = _sparse[page][offset];
            if (index >= 0 && index < _count && _entities[index] == entityId)
            {
                component = _dense[index];
                return true;
            }
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

    private void EnsurePage(int page)
    {
        if (page >= _sparse.Length)
        {
            int newLength = _sparse.Length == 0 ? 16 : _sparse.Length * 2;
            while (newLength <= page)
                newLength *= 2;
            Array.Resize(ref _sparse, newLength);
        }

        if (_sparse[page] == null)
        {
            var pageArray = new int[PageSize];
            for (int i = 0; i < PageSize; i++)
                pageArray[i] = -1;
            _sparse[page] = pageArray;
        }
    }
}
