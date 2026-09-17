namespace GreenCake;

using System;

public struct RTHandle
{
    internal int id;

    public bool IsValid => id >= 0;

    public void Resize(int width, int height)
    {
        NativeCalls.SrpRtResize(id, (uint)width, (uint)height);
    }
}

public struct MeshHandle
{
    internal int id;

    public bool IsValid => id != 0;
}

public struct MaterialHandle
{
    internal int id;

    public bool IsValid => id != 0;
}
