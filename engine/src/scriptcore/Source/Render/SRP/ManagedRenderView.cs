namespace GreenCake;

using System.Numerics;

public readonly struct ManagedRenderView
{
    private readonly ulong _handle;

    internal ManagedRenderView(ulong handle)
    {
        _handle = handle;
    }

    public unsafe Matrix4x4 ViewMatrix => ReadMatrix(0);
    public unsafe Matrix4x4 ProjectionMatrix => ReadMatrix(1);
    public unsafe Matrix4x4 ViewProjectionMatrix => ReadMatrix(2);

    public unsafe Vector3 CameraPosition
    {
        get
        {
            float* position = stackalloc float[3];
            NativeCalls.SrpViewPosition(_handle, position);
            return new Vector3(position[0], position[1], position[2]);
        }
    }

    public unsafe int ViewportX
    {
        get
        {
            int* rect = stackalloc int[4];
            NativeCalls.SrpViewViewport(_handle, rect);
            return rect[0];
        }
    }

    public unsafe int ViewportY
    {
        get
        {
            int* rect = stackalloc int[4];
            NativeCalls.SrpViewViewport(_handle, rect);
            return rect[1];
        }
    }

    public unsafe int ViewportWidth
    {
        get
        {
            int* rect = stackalloc int[4];
            NativeCalls.SrpViewViewport(_handle, rect);
            return rect[2];
        }
    }

    public unsafe int ViewportHeight
    {
        get
        {
            int* rect = stackalloc int[4];
            NativeCalls.SrpViewViewport(_handle, rect);
            return rect[3];
        }
    }

    private unsafe Matrix4x4 ReadMatrix(int which)
    {
        float* values = stackalloc float[16];
        NativeCalls.SrpViewMatrix(_handle, which, values);
        var matrix = new Matrix4x4(
            values[0], values[1], values[2], values[3],
            values[4], values[5], values[6], values[7],
            values[8], values[9], values[10], values[11],
            values[12], values[13], values[14], values[15]);
        return Matrix4x4.Transpose(matrix);
    }
}
