
StructuredBuffer<float>   Input  : register(t0);
RWStructuredBuffer<float> Output : register(u0);

cbuffer params : register(b0)
{
    uint SizeH;
    uint SizeW;
    uint StrideI;
    uint StrideO;
}

static const uint NumThreads = 64;

[numthreads(64 ,1 ,1)]
void main(uint3 threadId : SV_GroupThreadID, uint groupId : SV_GroupID)
{
    const uint id = groupId.x * NumThreads + threadId.x;
    
    if(id < SizeH * SizeW)
    {
        Output[id] = Input[id];
    }
}

