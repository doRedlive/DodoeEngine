namespace GreenCake;

using System;

internal interface ICakeComponent
{
}

public abstract class CakeComponent : ICakeComponent
{
    public Entity Entity { get; internal set; } = null!;
}

public abstract class NativeComponent : CakeComponent
{
}

public class IDComponent : NativeComponent
{
    public ulong ID => NativeCalls.Native_IDComponentGetID(Entity.ID);

    public string Name
    {
        get => NativeCalls.Native_IDComponentGetName(Entity.ID);
        set => NativeCalls.Native_IDComponentSetName(Entity.ID, value);
    }
}
