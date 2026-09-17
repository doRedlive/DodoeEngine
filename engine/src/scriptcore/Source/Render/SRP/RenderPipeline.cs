namespace GreenCake;

using System.Collections.Generic;

public sealed class RenderPipelineDefinition
{
    public string Type { get; internal set; } = "Deferred";
}

public abstract class RenderPipeline : System.IDisposable
{
    internal readonly List<RenderFeature> Features = new();
    internal PipelineServices Services = null!;

    public abstract void Initialize(RenderPipelineDefinition definition, PipelineServices services);

    public virtual void OnResize(int width, int height)
    {
        foreach (var feature in Features)
        {
            feature.OnResize(width, height);
        }
    }

    public virtual void Dispose()
    {
        for (var index = Features.Count - 1; index >= 0; index--)
        {
            Features[index].Dispose();
        }
        Features.Clear();
    }

    protected void AddFeature(RenderFeature feature)
    {
        feature.Handle = ScriptHub.NextFeatureHandle();
        feature.Pipeline = this;
        Features.Add(feature);
    }
}

public abstract class RenderFeature : System.IDisposable
{
    internal int Handle;
    internal RenderPipeline Pipeline = null!;

    public abstract RenderPhase Phase { get; }

    public virtual void Initialize(PipelineServices services)
    {
    }

    public virtual void OnResize(int width, int height)
    {
    }

    public abstract void AddRenderPasses(RenderGraph graph, PassBuildContext context);

    public virtual void Dispose()
    {
    }
}

public abstract class RenderPass
{
    public abstract void Build(RenderGraph graph, PassBuildContext context);
}

public readonly struct PassBuildContext
{
    public ManagedRenderView View { get; }

    internal PassBuildContext(ManagedRenderView view)
    {
        View = view;
    }
}
