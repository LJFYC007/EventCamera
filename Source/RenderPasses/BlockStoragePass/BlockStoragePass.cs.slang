Texture2D<float4> input;
RWTexture2DArray<float4> output;

cbuffer PerFrameCB
{
    uint2 gResolution;
    uint frame_id;
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint index = dispatchThreadID.y * gResolution.x + dispatchThreadID.x;
    if (dispatchThreadID.x < gResolution.x && dispatchThreadID.y < gResolution.y)
    {
        uint2 tex_coord2 = uint2(dispatchThreadID.xy);
        float4 color = input[tex_coord2];
        uint3 tex_coord = uint3(tex_coord2, frame_id);
        output[tex_coord] = color;
    }
}
