namespace GreenCake;

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static partial class ScriptHub
{
    private static readonly Dictionary<int, RenderPipeline> SrpPipelines = new();
    private static readonly Dictionary<int, RenderFeature> SrpFeatures = new();
    private static readonly Dictionary<int, Action<RasterCommandContext>> SrpExecuteCallbacks = new();
    private static int SrpNextPipelineId = 1;
    private static int SrpNextFeatureId = 1;
    private static int SrpNextExecuteId = 1;

    internal static int NextFeatureHandle()
    {
        return SrpNextFeatureId++;
    }

    internal static int RegisterExecuteCallback(Action<RasterCommandContext> callback)
    {
        var id = SrpNextExecuteId++;
        SrpExecuteCallbacks[id] = callback;
        return id;
    }

    private static unsafe int SrpCreatePipeline(void** args, void** result)
    {
        var type = Marshal.PtrToStringUTF8((IntPtr)args[0]) ?? "Deferred";
        RenderPipeline pipeline = type switch
        {
            "Deferred" => new DeferredPipeline(),
            _ => new DeferredPipeline(),
        };
        pipeline.Services = new PipelineServices();
        pipeline.Initialize(new RenderPipelineDefinition { Type = type }, pipeline.Services);

        var id = SrpNextPipelineId++;
        SrpPipelines[id] = pipeline;
        foreach (var feature in pipeline.Features)
        {
            SrpFeatures[feature.Handle] = feature;
        }
        *(int*)result[0] = id;
        return 1;
    }

    private static unsafe int SrpShutdownPipeline(void** args)
    {
        var id = *(int*)args[0];
        if (SrpPipelines.Remove(id, out var pipeline))
        {
            foreach (var feature in pipeline.Features)
            {
                SrpFeatures.Remove(feature.Handle);
            }
            pipeline.Dispose();
        }
        SrpExecuteCallbacks.Clear();
        return 1;
    }

    private static unsafe int SrpGetFeatureCount(void** args, void** result)
    {
        var id = *(int*)args[0];
        var count = SrpPipelines.TryGetValue(id, out var pipeline) ? pipeline.Features.Count : 0;
        *(int*)result[0] = count;
        return 1;
    }

    private static unsafe int SrpGetFeature(void** args, void** result)
    {
        var pipelineId = *(int*)args[0];
        var index = *(int*)args[1];
        var feature = SrpPipelines[pipelineId].Features[index];
        *(int*)result[0] = feature.Handle;
        *(int*)result[1] = (int)feature.Phase;
        return 1;
    }

    private static unsafe int SrpFeatureInitialize(void** args)
    {
        var id = *(int*)args[0];
        var feature = SrpFeatures[id];
        feature.Initialize(feature.Pipeline.Services);
        return 1;
    }

    private static unsafe int SrpFeatureOnResize(void** args)
    {
        var id = *(int*)args[0];
        var width = *(int*)args[1];
        var height = *(int*)args[2];
        SrpFeatures[id].OnResize(width, height);
        return 1;
    }

    private static unsafe int SrpFeatureDispose(void** args)
    {
        var id = *(int*)args[0];
        if (SrpFeatures.Remove(id, out var feature))
        {
            feature.Dispose();
        }
        return 1;
    }

    private static unsafe int SrpFeatureAddPasses(void** args)
    {
        var id = *(int*)args[0];
        var graph = new RenderGraph(*(ulong*)args[1]);
        var view = new ManagedRenderView(*(ulong*)args[2]);
        SrpFeatures[id].AddRenderPasses(graph, new PassBuildContext(view));
        return 1;
    }

    private static unsafe int SrpExecute(void** args)
    {
        var id = *(int*)args[0];
        SrpExecuteCallbacks[id](default);
        return 1;
    }

    private static unsafe int SrpClearExecuteCallbacks(void** args)
    {
        SrpExecuteCallbacks.Clear();
        SrpNextExecuteId = 1;
        return 1;
    }
}
