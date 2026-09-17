namespace GreenCake;

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

internal static unsafe partial class NativeCalls
{
    private static readonly Dictionary<string, IntPtr> SrpUtf8Cache = new();

    private static byte* SrpUtf8(string? value)
    {
        var key = value ?? string.Empty;
        if (SrpUtf8Cache.TryGetValue(key, out var cached))
        {
            return (byte*)cached;
        }
        var allocated = Marshal.StringToCoTaskMemUTF8(key);
        SrpUtf8Cache[key] = allocated;
        return (byte*)allocated;
    }

    internal static void SrpCmdDrawRenderers(string phase, int queueMin, int queueMax, int layerMask)
    {
        b->native_srp_cmd_draw_renderers(SrpUtf8(phase), queueMin, queueMax, layerMask);
    }

    internal static void SrpCmdDrawMesh(int mesh, int subMesh, int material, float* transform)
    {
        b->native_srp_cmd_draw_mesh(mesh, subMesh, material, transform);
    }

    internal static void SrpCmdDrawProcedural(int material)
    {
        b->native_srp_cmd_draw_procedural(material);
    }

    internal static void SrpCmdClearColor(int slot, float r, float g, float b1, float a)
    {
        b->native_srp_cmd_clear_color(slot, r, g, b1, a);
    }

    internal static void SrpCmdClearDepth(float depth, int stencil)
    {
        b->native_srp_cmd_clear_depth(depth, stencil);
    }

    internal static uint SrpGraphCreateTexture(ulong graph, string name, uint width, uint height, int format, bool depth, uint sampleCount)
    {
        return b->native_srp_graph_create_texture(graph, SrpUtf8(name), width, height, format, depth ? 1 : 0, sampleCount);
    }

    internal static uint SrpGraphImportRt(ulong graph, int renderTarget, string name, bool depth)
    {
        return b->native_srp_graph_import_rt(graph, renderTarget, SrpUtf8(name), depth ? 1 : 0);
    }

    internal static uint SrpGraphImportBackBuffer(ulong graph, string name)
    {
        return b->native_srp_graph_import_backbuffer(graph, SrpUtf8(name));
    }

    internal static void SrpGraphAddRasterPass(ulong graph, string name, int phase, int executeId,
        int[] colorHandles, int[] colorLoads, float[] colorClears, int colorCount,
        int depthHandle, int depthLoad, float depthClear, int[] readHandles, int readCount)
    {
        fixed (int* handles = colorHandles)
        fixed (int* loads = colorLoads)
        fixed (float* clears = colorClears)
        fixed (int* reads = readHandles)
        {
            b->native_srp_graph_add_raster_pass(graph, SrpUtf8(name), phase, executeId,
                handles, loads, clears, colorCount, depthHandle, depthLoad, depthClear, reads, readCount);
        }
    }

    internal static int SrpCreateRenderTarget(string name, int format, float scaleX, float scaleY)
    {
        return b->native_srp_create_render_target(SrpUtf8(name), format, scaleX, scaleY);
    }

    internal static int SrpFindRenderTarget(string name)
    {
        return b->native_srp_find_render_target(SrpUtf8(name));
    }

    internal static bool SrpRtIsValid(int id)
    {
        return b->native_srp_rt_is_valid(id) != 0;
    }

    internal static void SrpRtResize(int id, uint width, uint height)
    {
        b->native_srp_rt_resize(id, width, height);
    }

    internal static void SrpViewMatrix(ulong view, int which, float* outValues)
    {
        b->native_srp_view_matrix(view, which, outValues);
    }

    internal static void SrpViewViewport(ulong view, int* outValues)
    {
        b->native_srp_view_viewport(view, outValues);
    }

    internal static void SrpViewPosition(ulong view, float* outValues)
    {
        b->native_srp_view_position(view, outValues);
    }
}
