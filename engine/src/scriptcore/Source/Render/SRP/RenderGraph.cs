namespace GreenCake;

using System;
using System.Collections.Generic;

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
    internal readonly List<int> ColorHandles = new();
    internal readonly List<int> ColorLoads = new();
    internal readonly List<float> ColorClears = new();
    internal readonly List<int> ReadHandles = new();

    internal int DepthHandle = -1;
    internal int DepthLoad = (int)LoadAction.DontCare;
    internal float DepthClear = 1.0f;

    public RasterPassDesc SetColorAttachment(RenderGraphTexture texture, int slot, LoadAction load, Color clear)
    {
        while (ColorHandles.Count <= slot)
        {
            ColorHandles.Add(RenderGraphTexture.Invalid.index);
            ColorLoads.Add((int)LoadAction.DontCare);
            ColorClears.Add(0.0f);
            ColorClears.Add(0.0f);
            ColorClears.Add(0.0f);
            ColorClears.Add(0.0f);
        }
        ColorHandles[slot] = texture.index;
        ColorLoads[slot] = (int)load;
        ColorClears[slot * 4 + 0] = clear.r;
        ColorClears[slot * 4 + 1] = clear.g;
        ColorClears[slot * 4 + 2] = clear.b;
        ColorClears[slot * 4 + 3] = clear.a;
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
        ReadHandles.Add(texture.index);
        return this;
    }
}

public sealed class RenderGraph
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
            desc.ColorHandles.ToArray(), desc.ColorLoads.ToArray(), desc.ColorClears.ToArray(), desc.ColorHandles.Count,
            desc.DepthHandle, desc.DepthLoad, desc.DepthClear,
            desc.ReadHandles.ToArray(), desc.ReadHandles.Count);
    }
}
