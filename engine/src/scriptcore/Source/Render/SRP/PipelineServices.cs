namespace GreenCake;

public sealed class PipelineServices
{
    public RTHandle CreateRenderTarget(string name, SrpFormat format, float scaleX = 1.0f, float scaleY = 1.0f)
    {
        return new RTHandle { id = NativeCalls.SrpCreateRenderTarget(name, (int)format, scaleX, scaleY) };
    }

    public RTHandle FindRenderTarget(string name)
    {
        return new RTHandle { id = NativeCalls.SrpFindRenderTarget(name) };
    }

    public MeshHandle LoadMesh(string path)
    {
        var mesh = Resources.Load<Mesh>(path);
        return new MeshHandle { id = mesh?.InstanceID ?? 0 };
    }

    public MaterialHandle LoadMaterial(string path)
    {
        var material = Resources.Load<Material>(path);
        return new MaterialHandle { id = material?.InstanceID ?? 0 };
    }
}
