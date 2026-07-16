namespace GreenCake;

public class CakeSystem
{
    protected World World { get; private set; } = World.Current;

    public virtual void OnCreate() { }
    public virtual void OnUpdate() { }
    public virtual void OnDestroy() { }
}
