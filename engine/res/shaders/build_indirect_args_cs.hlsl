struct DrawIndexedIndirectArgs
{
    uint indexCount;
    uint instanceCount;
    uint startIndexLocation;
    int  baseVertexLocation;
    uint startInstanceLocation;
};

RWStructuredBuffer<uint>              VisibleObjects : register(u0);
RWStructuredBuffer<uint>              VisibleCount   : register(u1);
RWStructuredBuffer<DrawIndexedIndirectArgs> IndirectArgs   : register(u2);

uint object_count;
uint padding0;
uint padding1;
uint padding2;

[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint idx = dtid.x;
    if (idx >= VisibleCount[0])
        return;

    uint object_idx = VisibleObjects[idx];

    DrawIndexedIndirectArgs args;
    args.indexCount          = 6;
    args.instanceCount       = 1;
    args.startIndexLocation  = 0;
    args.baseVertexLocation  = 0;
    args.startInstanceLocation = object_idx;

    IndirectArgs[idx] = args;
}
