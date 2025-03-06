Texture2D<float4> input;
RWStructuredBuffer<uint> buffer_output;

cbuffer PerFrameCB
{
    uint2 gResolution;
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint index = dispatchThreadID.y * gResolution.x + dispatchThreadID.x;
    if (dispatchThreadID.x < gResolution.x && dispatchThreadID.y < gResolution.y)
    {
        int2 tex_coord = dispatchThreadID.xy;
        float3 color = input[tex_coord].rgb;

        if (color.x > 0.f)
        {
            uint id = buffer_output.IncrementCounter();
            buffer_output[id] = index * 2;
        }
        else if (color.y > 0.f)
        {
            uint id = buffer_output.IncrementCounter();
            buffer_output[id] = index * 2 + 1;
        }
    }
}
