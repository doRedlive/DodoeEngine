namespace GreenCake;

public struct RenderQueueRange
{
    public int Min;
    public int Max;

    public static RenderQueueRange Opaque => new(0, 2500);
    public static RenderQueueRange Transparent => new(2501, 5000);

    public RenderQueueRange(int min, int max)
    {
        Min = min;
        Max = max;
    }
}

public struct MeshDrawSettings
{
    public string Phase;
    public RenderQueueRange Queue;
    public int LayerMask;
    public MeshHandle MaterialOverride;
    public bool EnableInstancing;

    public static MeshDrawSettings For(string phase)
    {
        return new MeshDrawSettings { Phase = phase, Queue = RenderQueueRange.Opaque, LayerMask = ~0 };
    }

    public MeshDrawSettings WithQueue(RenderQueueRange queue)
    {
        Queue = queue;
        return this;
    }

    public MeshDrawSettings WithQueue(int min, int max)
    {
        Queue = new RenderQueueRange(min, max);
        return this;
    }

    public MeshDrawSettings WithLayerMask(int mask)
    {
        LayerMask = mask;
        return this;
    }

    public MeshDrawSettings WithMaterial(MaterialHandle material)
    {
        MaterialOverride = material;
        return this;
    }
}
