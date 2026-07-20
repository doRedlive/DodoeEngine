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

struct GpuTransform
{
    float4x4 local_to_world;
    float4x4 world_to_local;
};

struct BucketCount
{
    uint instance_count;
    uint current_offset;
    uint pad0;
    uint pad1;
};

struct DrawIndexedIndirectArgs
{
    uint indexCount;
    uint instanceCount;
    uint startIndexLocation;
    int  baseVertexLocation;
    uint startInstanceLocation;
};

StructuredBuffer<uint>               VisibleObjects  : register(t0);
StructuredBuffer<GpuObjectMeta>      ObjectMetaBuffer : register(t1);
StructuredBuffer<GpuTransform>       TransformBuffer  : register(t2);
StructuredBuffer<BucketCount>        BucketCounts     : register(t3);

RWStructuredBuffer<DrawIndexedIndirectArgs> IndirectArgs : register(u0);
RWStructuredBuffer<BucketCount>             BucketOffsets : register(u1);

uint getBucketKey(GpuObjectMeta meta)
{
    return meta.type & 0xFF;
}

[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint idx = dtid.x;
    uint object_idx = VisibleObjects[idx];
    GpuObjectMeta meta = ObjectMetaBuffer[object_idx];
    uint bucket = getBucketKey(meta);

    uint slot;
    InterlockedAdd(BucketCounts[bucket].current_offset, 1, slot);

    DrawIndexedIndirectArgs args;
    args.indexCount          = 6;
    args.instanceCount       = 1;
    args.startIndexLocation  = 0;
    args.baseVertexLocation  = 0;
    args.startInstanceLocation = object_idx;

    IndirectArgs[slot] = args;
    BucketOffsets[bucket + 1].instance_count = BucketCounts[bucket].current_offset;
}
