namespace GreenCake;

using System.Numerics;

public sealed class MaterialPropertyBlock
{
    public Color Color { get; set; } = Color.White;
}

public readonly struct RasterCommandContext
{
    public void DrawRenderers(MeshDrawSettings settings)
    {
        NativeCalls.SrpCmdDrawRenderers(settings.Phase, settings.Queue.Min, settings.Queue.Max, settings.LayerMask);
    }

    public unsafe void DrawMesh(MeshHandle mesh, int subMesh, MaterialHandle material, Matrix4x4 transform, MaterialPropertyBlock? properties = null)
    {
        float* values = stackalloc float[16];
        values[0] = transform.M11; values[1] = transform.M12; values[2] = transform.M13; values[3] = transform.M14;
        values[4] = transform.M21; values[5] = transform.M22; values[6] = transform.M23; values[7] = transform.M24;
        values[8] = transform.M31; values[9] = transform.M32; values[10] = transform.M33; values[11] = transform.M34;
        values[12] = transform.M41; values[13] = transform.M42; values[14] = transform.M43; values[15] = transform.M44;
        NativeCalls.SrpCmdDrawMesh(mesh.id, subMesh, material.id, values);
    }

    public void DrawProcedural(MaterialHandle material)
    {
        NativeCalls.SrpCmdDrawProcedural(material.id);
    }

    public void ClearColor(int attachmentIndex, Color color)
    {
        NativeCalls.SrpCmdClearColor(attachmentIndex, color.r, color.g, color.b, color.a);
    }

    public void ClearDepth(float depth, byte stencil = 0)
    {
        NativeCalls.SrpCmdClearDepth(depth, stencil);
    }
}

public readonly struct ComputeCommandContext
{
    public void SetTexture(int slot, RenderGraphTexture texture)
    {
    }

    public void SetBuffer(int slot, RenderGraphBuffer buffer)
    {
    }

    public void Dispatch(int x, int y, int z)
    {
    }
}
