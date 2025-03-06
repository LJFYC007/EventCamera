Texture2D<float4> input;
RWStructuredBuffer<uint4> buffer_output;

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
            buffer_output[id] = uint4(tex_coord.xy, 0, 0);
        }
        else if (color.y > 0.f)
        {
            uint id = buffer_output.IncrementCounter();
            buffer_output[id] = uint4(tex_coord.xy, 1, 0);
        }
    }
}
