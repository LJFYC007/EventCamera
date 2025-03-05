Texture2D<float4> input;
RWStructuredBuffer<uint4> buffer_output;

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint index = dispatchThreadID.y * 1280 + dispatchThreadID.x;
    if (dispatchThreadID.x < 1280 && dispatchThreadID.y < 720)
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
