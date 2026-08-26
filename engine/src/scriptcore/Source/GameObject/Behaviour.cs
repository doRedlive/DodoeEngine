namespace GreenCake;

using System;
using System.Collections;

public abstract class CakeBehaviour : CakeComponent
{
    internal bool _awakeCalled;
    internal bool _startCalled;
    internal bool _enabled = true;
    internal bool _destroyed;

    public bool Enabled
    {
        get => _enabled;
        set
        {
            if (_enabled == value) return;
            _enabled = value;
            if (!_destroyed && GameObject != null && GameObject.ActiveInHierarchy)
            {
                if (_enabled) try { OnEnable(); } catch (Exception e) { Debug.LogError($"OnEnable error: {e}"); }
                else try { OnDisable(); } catch (Exception e) { Debug.LogError($"OnDisable error: {e}"); }
            }
        }
    }

    public GameObject GameObject { get; internal set; }

    private TransformComponent _transform;
    public TransformComponent Transform =>
        _transform ??= GameObject?.Entity?.GetComponent<TransformComponent>();

    public new Entity Entity
    {
        get => base.Entity;
        set
        {
            base.Entity = value;
            _transform = null;
        }
    }

    public virtual void Awake() { }
    public virtual void Start() { }
    public virtual void Update() { }
    public virtual void FixedUpdate() { }
    public virtual void LateUpdate() { }
    public virtual void OnEnable() { }
    public virtual void OnDisable() { }
    public virtual void OnDestroy() { }

    public virtual void OnCollisionEnter2D(Collision2D collision) { }
    public virtual void OnCollisionStay2D(Collision2D collision) { }
    public virtual void OnCollisionExit2D(Collision2D collision) { }
    public virtual void OnTriggerEnter2D(NativeComponent other) { }
    public virtual void OnTriggerStay2D(NativeComponent other) { }
    public virtual void OnTriggerExit2D(NativeComponent other) { }

    protected Coroutine StartCoroutine(IEnumerator routine)
    {
        if (routine == null) return null;
        return CoroutineManager.Start(this, routine);
    }

    protected void StopCoroutine(Coroutine coroutine)
    {
        if (coroutine != null) CoroutineManager.Stop(coroutine);
    }

    protected void StopAllCoroutines()
    {
        CoroutineManager.StopAll(this);
    }

    public T GetComponent<T>() where T : CakeComponent => GameObject?.GetComponent<T>();
    public T GetOrAddComponent<T>() where T : CakeComponent, new()
    {
        var c = GetComponent<T>();
        if (c == null && GameObject != null) c = AddComponent<T>();
        return c;
    }
    public T[] GetComponentsInChildren<T>(bool includeInactive = false) where T : CakeComponent =>
        GameObject != null ? GameObject.GetComponentsInChildren<T>(includeInactive) : Array.Empty<T>();
    public T GetComponentInParent<T>() where T : CakeComponent => GameObject?.GetComponentInParent<T>();
    public T AddComponent<T>() where T : CakeComponent, new() => GameObject.AddComponent<T>();
}
