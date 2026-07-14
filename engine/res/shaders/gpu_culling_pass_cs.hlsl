struct GpuObjectMeta
{
    uint type;
    uint flags;
    uint material_id;
    uint texture_id;
    uint bounds_id;
    uint data_offset;
    uint reserved0;
    uint reserved1;
};

struct GpuBounds
{
    float3 center;
    float  sphere_radius;
    float3 extent;
    float  _pad;
};

struct GpuTransform
{
    float4x4 local_to_world;
    float4x4 world_to_local;
};

cbuffer CullingParams : register(b0)
{
    float4 frustum_planes[6];
    uint   object_count;
    uint3  padding;
};

StructuredBuffer<GpuObjectMeta> ObjectMetaBuffer  : register(t0);
StructuredBuffer<GpuTransform>  TransformBuffer   : register(t1);
StructuredBuffer<GpuBounds>     BoundsBuffer      : register(t2);

RWStructuredBuffer<uint> VisibleObjects : register(u0);
RWStructuredBuffer<uint> VisibleCount   : register(u1);

bool testPlane(float4 plane, float3 center, float3 extent)
{
    float r = abs(extent.x * plane.x) + abs(extent.y * plane.y) + abs(extent.z * plane.z);
    float s = dot(plane.xyz, center) + plane.w;
    return s > -r;
}

[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint idx = dtid.x;
    if (idx >= object_count)
        return;

    GpuObjectMeta meta = ObjectMetaBuffer[idx];
    if ((meta.flags & 1) == 0)
        return;

    GpuBounds bounds = BoundsBuffer[idx];

    bool visible = true;
    [unroll]
    for (int i = 0; i < 6; i++)
    {
        if (!testPlane(frustum_planes[i], bounds.center, bounds.extent))
        {
            visible = false;
            break;
        }
    }

    if (visible)
    {
        uint out_idx;
        InterlockedAdd(VisibleCount[0], 1, out_idx);
        VisibleObjects[out_idx] = idx;
    }
}
