namespace GreenCake;

using System;

public struct RenderGraphTexture : IEquatable<RenderGraphTexture>
{
    internal int index;

    public bool IsValid => index >= 0;

    public static RenderGraphTexture Invalid => new() { index = -1 };

    public bool Equals(RenderGraphTexture other) => index == other.index;
    public override bool Equals(object? obj) => obj is RenderGraphTexture other && Equals(other);
    public override int GetHashCode() => index;
}

public struct RenderGraphBuffer : IEquatable<RenderGraphBuffer>
{
    internal int index;

    public bool IsValid => index >= 0;

    public static RenderGraphBuffer Invalid => new() { index = -1 };

    public bool Equals(RenderGraphBuffer other) => index == other.index;
    public override bool Equals(object? obj) => obj is RenderGraphBuffer other && Equals(other);
    public override int GetHashCode() => index;
}

public sealed class RasterPassDesc
{
    internal int[] ColorHandles = Array.Empty<int>();
    internal int[] ColorLoads = Array.Empty<int>();
    internal float[] ColorClears = Array.Empty<float>();
    internal int ColorCount;
    internal int[] ReadHandles = Array.Empty<int>();
    internal int ReadCount;

    internal int DepthHandle = -1;
    internal int DepthLoad = (int)LoadAction.DontCare;
    internal float DepthClear = 1.0f;

    public void Reset()
    {
        ColorCount = 0;
        ReadCount = 0;
        DepthHandle = -1;
        DepthLoad = (int)LoadAction.DontCare;
        DepthClear = 1.0f;
    }

    public RasterPassDesc SetColorAttachment(RenderGraphTexture texture, int slot, LoadAction load, Color clear)
    {
        EnsureColorCapacity(slot + 1);
        ColorHandles[slot] = texture.index;
        ColorLoads[slot] = (int)load;
        ColorClears[slot * 4 + 0] = clear.r;
        ColorClears[slot * 4 + 1] = clear.g;
        ColorClears[slot * 4 + 2] = clear.b;
        ColorClears[slot * 4 + 3] = clear.a;
        if (slot + 1 > ColorCount)
        {
            ColorCount = slot + 1;
        }
        return this;
    }

    public RasterPassDesc SetDepthAttachment(RenderGraphTexture texture, LoadAction load, float clearDepth = 1.0f)
    {
        DepthHandle = texture.index;
        DepthLoad = (int)load;
        DepthClear = clearDepth;
        return this;
    }

    public RasterPassDesc ReadTexture(RenderGraphTexture texture)
    {
        if (ReadCount == ReadHandles.Length)
        {
            Array.Resize(ref ReadHandles, Math.Max(ReadCount + 1, 4));
        }
        ReadHandles[ReadCount++] = texture.index;
        return this;
    }

    private void EnsureColorCapacity(int count)
    {
        if (ColorHandles.Length >= count)
        {
            return;
        }
        var capacity = Math.Max(count, 4);
        Array.Resize(ref ColorHandles, capacity);
        Array.Resize(ref ColorLoads, capacity);
        Array.Resize(ref ColorClears, capacity * 4);
    }
}

public readonly struct RenderGraph
{
    internal ulong Handle { get; }

    internal RenderGraph(ulong handle)
    {
        Handle = handle;
    }

    public RenderGraphTexture CreateTexture(string name, int width, int height, SrpFormat format, bool depth = false, int sampleCount = 1)
    {
        var index = (int)NativeCalls.SrpGraphCreateTexture(Handle, name, (uint)width, (uint)height, (int)format, depth, (uint)sampleCount);
        return new RenderGraphTexture { index = index };
    }

    public RenderGraphTexture ImportTexture(RTHandle renderTarget, string name, bool depth = false)
    {
        var index = (int)NativeCalls.SrpGraphImportRt(Handle, renderTarget.id, name, depth);
        return new RenderGraphTexture { index = index };
    }

    public RenderGraphTexture ImportBackBuffer(string name)
    {
        var index = (int)NativeCalls.SrpGraphImportBackBuffer(Handle, name);
        return new RenderGraphTexture { index = index };
    }

    public void AddRasterPass(string name, RenderPhase phase, RasterPassDesc desc, Action<RasterCommandContext> execute)
    {
        var executeId = ScriptHub.RegisterExecuteCallback(execute);
        NativeCalls.SrpGraphAddRasterPass(
            Handle, name, (int)phase, executeId,
            desc.ColorHandles, desc.ColorLoads, desc.ColorClears, desc.ColorCount,
            desc.DepthHandle, desc.DepthLoad, desc.DepthClear,
            desc.ReadHandles, desc.ReadCount);
    }
}
