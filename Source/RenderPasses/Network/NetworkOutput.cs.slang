RWStructuredBuffer<half> input;
RWTexture2D<float4> output;
RWStructuredBuffer<uint2> buffer_output;

cbuffer PerFrameCB
{
    uint2 gResolution;
    uint gNetworkInputLength;
    uint gFrame;
}

[numthreads(32, 32, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    if (dispatchThreadID.x < gResolution.x && dispatchThreadID.y < gResolution.y)
    {
        uint2 tex_coord2 = uint2(dispatchThreadID.xy);
        uint index = (dispatchThreadID.x * gResolution.y + dispatchThreadID.y) * gNetworkInputLength;
        float color = input[index + gNetworkInputLength / 2];

        index = dispatchThreadID.y * gResolution.x + dispatchThreadID.x;
        if (color > 0.f)
        {
            uint id = buffer_output.IncrementCounter();
            buffer_output[id] = uint2(gFrame, index * 2 + 1);
            output[tex_coord2] = float4(1.f, 0.f, 0.f, 1.f);
        }
        else if (color < 0.f)
        {
            uint id = buffer_output.IncrementCounter();
            buffer_output[id] = uint2(gFrame, index * 2);
            output[tex_coord2] = float4(0.f, 0.f, 1.f, 1.f);
        }
        else
            output[tex_coord2] = float4(1.f, 1.f, 1.f, 1.f);
    }
}
