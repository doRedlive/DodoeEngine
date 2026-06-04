namespace GreenCake;

public class MonoBehaviour : Component
{
    public virtual void Awake() { }
    public virtual void Start() { }
    public virtual void Update() { }
    public virtual void OnDestroy() { }
    public virtual void OnEnable() { }
    public virtual void OnDisable() { }

    public bool Enabled { get; set; } = true;
    public GameObject GameObject { get; internal set; }
    public Transform Transform { get { return GameObject != null ? GameObject.Transform : null; } }

    internal bool _awakeCalled;
    internal bool _startCalled;
    internal bool _destroyed;
}
