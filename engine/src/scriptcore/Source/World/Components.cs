namespace GreenCake;

using System;

public abstract class CakeComponent
{
    public Entity Entity { get; set; }
}

public abstract class NativeComponent : CakeComponent
{
}
