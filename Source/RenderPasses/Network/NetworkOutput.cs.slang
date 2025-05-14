RWStructuredBuffer<float> input;
RWTexture2D<float4> output;

cbuffer PerFrameCB
{
    uint2 gResolution;
    uint gNetworkInputLength;
}

[numthreads(32, 32, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint index = dispatchThreadID.y * gResolution.x + dispatchThreadID.x;
    if (dispatchThreadID.x < gResolution.x && dispatchThreadID.y < gResolution.y)
    {
        uint2 tex_coord2 = uint2(dispatchThreadID.xy);
        uint index = (dispatchThreadID.x * gResolution.y + dispatchThreadID.y) * gNetworkInputLength * 2;
        float color = input[index + gNetworkInputLength / 2] - input[index + gNetworkInputLength + gNetworkInputLength / 2];
        if (color > 0.f)
            output[tex_coord2] = float4(1.f, 0.f, 0.f, 1.f);
        else if (color < 0.f)
            output[tex_coord2] = float4(0.f, 0.f, 1.f, 1.f);
        else
            output[tex_coord2] = float4(1.f, 1.f, 1.f, 1.f);
    }
}
