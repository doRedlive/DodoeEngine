namespace GreenCake;

public sealed class DeferredPipeline : RenderPipeline
{
    public override void Initialize(RenderPipelineDefinition definition, PipelineServices services)
    {
        AddFeature(new ExampleOverlayFeature());
    }
}

public sealed class ExampleOverlayFeature : RenderFeature
{
    public static bool EnableDraw = false;

    public override RenderPhase Phase => RenderPhase.PostProcess;

    public override void AddRenderPasses(RenderGraph graph, PassBuildContext context)
    {
        var width = context.View.ViewportWidth;
        var height = context.View.ViewportHeight;
        if (width <= 0 || height <= 0)
        {
            return;
        }

        var overlay = graph.CreateTexture("SrpOverlayColor", width, height, SrpFormat.RGBA8_UNORM);
        var desc = new RasterPassDesc()
            .SetColorAttachment(overlay, 0, LoadAction.Clear, new Color(0.0f, 0.0f, 0.0f, 0.0f));

        graph.AddRasterPass("SrpOverlayPass", RenderPhase.PostProcess, desc, cmd =>
        {
            cmd.ClearColor(0, new Color(0.0f, 0.0f, 0.0f, 0.0f));
            if (!EnableDraw)
            {
                return;
            }
            cmd.DrawRenderers(MeshDrawSettings.For("GBuffer").WithQueue(RenderQueueRange.Opaque));
        });
    }
}

public sealed class MyOutlineFeature : RenderFeature
{
    private RTHandle _outlineRT = null!;

    public override RenderPhase Phase => RenderPhase.PostProcess;

    public override void Initialize(PipelineServices services)
    {
        _outlineRT = services.CreateRenderTarget("SrpOutlineColor", SrpFormat.RGBA8_UNORM);
    }

    public override void OnResize(int width, int height)
    {
        _outlineRT.Resize(width, height);
    }

    public override void AddRenderPasses(RenderGraph graph, PassBuildContext context)
    {
        var outline = graph.ImportTexture(_outlineRT, "SrpOutlineColor");
        var desc = new RasterPassDesc()
            .SetColorAttachment(outline, 0, LoadAction.Clear, new Color(0.0f, 0.0f, 0.0f, 0.0f));
        graph.AddRasterPass("SrpOutlineMask", RenderPhase.PostProcess, desc, cmd =>
        {
            cmd.DrawRenderers(MeshDrawSettings.For("GBuffer").WithQueue(RenderQueueRange.Opaque));
        });
    }
}
