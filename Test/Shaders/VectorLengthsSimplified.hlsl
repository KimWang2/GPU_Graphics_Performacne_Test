struct Vector3D
{
    float x;
    float y;
    float z;
};

StructuredBuffer<Vector3D> InputVectors : register(t0);
RWStructuredBuffer<float> OutputLengths   : register(u0);

[numthreads(64 ,1 ,1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    if(tid.x < 64)
    {
        Vector3D vec = InputVectors[tid.x];
        float length = sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
        OutputLengths[tid.x] = length;
    }
}
    
