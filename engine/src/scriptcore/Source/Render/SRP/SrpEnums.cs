namespace GreenCake;

public enum RenderPhase : byte
{
    Shadow = 0,
    Opaque,
    Skybox,
    Lighting,
    Decals,
    Transparent,
    Sprite,
    Resolve,
    Taa,
    PostProcess,
    UI,
    EditorGizmo,
    DebugUI,
    Present,
}

public enum LoadAction : int
{
    Load = 0,
    Clear,
    DontCare,
}

public enum SrpFormat : int
{
    RGBA8_UNORM = 0,
    RGBA16_FLOAT = 1,
    RGBA32_FLOAT = 2,
    RG16_FLOAT = 3,
    D32 = 4,
}
