namespace GreenCake;

public class CakeBehaviour : CakeComponent
{
    public virtual void Awake() { }
    public virtual void Start() { }
    public virtual void Update() { }
    public virtual void OnDestroy() { }
    public virtual void OnEnable() { }
    public virtual void OnDisable() { }

    public bool Enabled { get; set; } = true;
    public GameObject GameObject { get; internal set; }
    public TransformComponent Transform { get { return GameObject != null ? GameObject.Transform : null; } }

    public T GetComponent<T>() where T : CakeComponent
    {
        return GameObject?.GetComponent<T>();
    }

    public T GetOrAddComponent<T>() where T : CakeComponent, new()
    {
        var comp = GameObject?.GetComponent<T>();
        if (comp != null)
            return comp;
        return GameObject?.AddComponent<T>();
    }

    internal bool _awakeCalled;
    internal bool _startCalled;
    internal bool _destroyed;
}
