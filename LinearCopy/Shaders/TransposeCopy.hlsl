
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
    const uint x = id % SizeW;
    const uint y = id / SizeW;
    
    // input is column major, output is row major
    if(id < SizeH * SizeW)
    {
        Output[y * StrideO + x] = Input[x * StrideI + y];
    }
}

