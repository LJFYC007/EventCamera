Texture2D<float4> input;
RWTexture2D<float> lastTexture;
RWStructuredBuffer<half> output;

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
        uint index = (dispatchThreadID.x * gResolution.y + dispatchThreadID.y) * gNetworkInputLength;
        float4 color = input[tex_coord2];
        for ( uint i = 0; i + 1 < gNetworkInputLength; i++ )
            output[index + i] = output[index + i + 1];
        float x = (0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b) * 255.f;
        float threshold = 3.f;
        float f = (1.f / threshold) * log(threshold);
        float y = x <= threshold ? x * f : log(x);
        float rounding = 1e8f;
        y = round(y * rounding) / rounding;
        output[index + gNetworkInputLength - 1] = y - lastTexture[tex_coord2];
        lastTexture[tex_coord2] = y;
    }
}
