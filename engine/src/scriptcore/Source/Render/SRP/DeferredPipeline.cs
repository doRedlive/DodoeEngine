namespace GreenCake;

using System;

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

    private readonly RasterPassDesc _desc = new();
    private readonly Action<RasterCommandContext> _execute;

    public ExampleOverlayFeature()
    {
        _execute = Execute;
    }

    public override RenderPhase Phase => RenderPhase.PostProcess;

    public override void AddRenderPasses(RenderGraph graph, PassBuildContext context)
    {
        var viewport = context.View.Viewport;
        if (viewport.width <= 0 || viewport.height <= 0)
        {
            return;
        }

        var overlay = graph.CreateTexture("SrpOverlayColor", viewport.width, viewport.height, SrpFormat.RGBA8_UNORM);
        _desc.Reset();
        _desc.SetColorAttachment(overlay, 0, LoadAction.Clear, new Color(0.0f, 0.0f, 0.0f, 0.0f));
        graph.AddRasterPass("SrpOverlayPass", RenderPhase.PostProcess, _desc, _execute);
    }

    private void Execute(RasterCommandContext cmd)
    {
        cmd.ClearColor(0, new Color(0.0f, 0.0f, 0.0f, 0.0f));
        if (!EnableDraw)
        {
            return;
        }
        cmd.DrawRenderers(MeshDrawSettings.For("GBuffer").WithQueue(RenderQueueRange.Opaque));
    }
}

public sealed class MyOutlineFeature : RenderFeature
{
    private RTHandle _outlineRT = null!;
    private readonly RasterPassDesc _desc = new();
    private readonly Action<RasterCommandContext> _execute;

    public MyOutlineFeature()
    {
        _execute = Execute;
    }

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
        _desc.Reset();
        _desc.SetColorAttachment(outline, 0, LoadAction.Clear, new Color(0.0f, 0.0f, 0.0f, 0.0f));
        graph.AddRasterPass("SrpOutlineMask", RenderPhase.PostProcess, _desc, _execute);
    }

    private void Execute(RasterCommandContext cmd)
    {
        cmd.DrawRenderers(MeshDrawSettings.For("GBuffer").WithQueue(RenderQueueRange.Opaque));
    }
}
