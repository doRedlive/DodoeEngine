namespace GreenCake;

using System;
using System.Runtime.InteropServices;

internal static unsafe partial class NativeCalls
{
    internal static void SrpCmdDrawRenderers(string phase, int queueMin, int queueMax, int layerMask)
    {
        var phasePtr = StrToPtr(phase);
        try { b->native_srp_cmd_draw_renderers(phasePtr, queueMin, queueMax, layerMask); }
        finally { Marshal.FreeCoTaskMem((IntPtr)phasePtr); }
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
        var namePtr = StrToPtr(name);
        try { return b->native_srp_graph_create_texture(graph, namePtr, width, height, format, depth ? 1 : 0, sampleCount); }
        finally { Marshal.FreeCoTaskMem((IntPtr)namePtr); }
    }

    internal static uint SrpGraphImportRt(ulong graph, int renderTarget, string name, bool depth)
    {
        var namePtr = StrToPtr(name);
        try { return b->native_srp_graph_import_rt(graph, renderTarget, namePtr, depth ? 1 : 0); }
        finally { Marshal.FreeCoTaskMem((IntPtr)namePtr); }
    }

    internal static uint SrpGraphImportBackBuffer(ulong graph, string name)
    {
        var namePtr = StrToPtr(name);
        try { return b->native_srp_graph_import_backbuffer(graph, namePtr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)namePtr); }
    }

    internal static void SrpGraphAddRasterPass(ulong graph, string name, int phase, int executeId,
        int[] colorHandles, int[] colorLoads, float[] colorClears, int colorCount,
        int depthHandle, int depthLoad, float depthClear, int[] readHandles, int readCount)
    {
        var namePtr = StrToPtr(name);
        fixed (int* handles = colorHandles)
        fixed (int* loads = colorLoads)
        fixed (float* clears = colorClears)
        fixed (int* reads = readHandles)
        {
            b->native_srp_graph_add_raster_pass(graph, namePtr, phase, executeId,
                handles, loads, clears, colorCount, depthHandle, depthLoad, depthClear, reads, readCount);
        }
        Marshal.FreeCoTaskMem((IntPtr)namePtr);
    }

    internal static int SrpCreateRenderTarget(string name, int format, float scaleX, float scaleY)
    {
        var namePtr = StrToPtr(name);
        try { return b->native_srp_create_render_target(namePtr, format, scaleX, scaleY); }
        finally { Marshal.FreeCoTaskMem((IntPtr)namePtr); }
    }

    internal static int SrpFindRenderTarget(string name)
    {
        var namePtr = StrToPtr(name);
        try { return b->native_srp_find_render_target(namePtr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)namePtr); }
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
