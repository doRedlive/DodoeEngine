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

struct BucketCount
{
    uint instance_count;
    uint current_offset;
    uint pad0;
    uint pad1;
};

StructuredBuffer<uint>          VisibleObjects  : register(t0);
StructuredBuffer<GpuObjectMeta> ObjectMetaBuffer : register(t1);

cbuffer BucketParams : register(b1)
{
    uint object_count;
    uint max_buckets;
    uint pad0;
    uint pad1;
};

RWStructuredBuffer<BucketCount> BucketCounts : register(u0);

uint getBucketKey(GpuObjectMeta meta)
{
    return meta.type & 0xFF;
}

[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint idx = dtid.x;
    if (idx >= object_count)
        return;

    uint object_idx = VisibleObjects[idx];
    GpuObjectMeta meta = ObjectMetaBuffer[object_idx];

    uint bucket = getBucketKey(meta);
    if (bucket >= max_buckets)
        return;

    InterlockedAdd(BucketCounts[bucket].instance_count, 1);
}
