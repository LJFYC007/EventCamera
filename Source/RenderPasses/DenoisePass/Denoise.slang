Texture2D<float4> input;
RWTexture2D<float4> color;
RWTexture2D<float4> output;
RWTexture2D<float> internalState;
RWTexture2D<float4> LastFrames[10];
RWStructuredBuffer<uint2> buffer_output;

cbuffer PerFrameCB
{
    uint2 gResolution;
    uint gFrame;
}

[numthreads(32, 32, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint index = dispatchThreadID.y * gResolution.x + dispatchThreadID.x;
    if (dispatchThreadID.x < gResolution.x && dispatchThreadID.y < gResolution.y)
    {
        uint2 pixel = uint2(dispatchThreadID.xy);
        for (int i = 0; i + 1 < 10; ++i)
            LastFrames[i][pixel] = LastFrames[i + 1][pixel];
        LastFrames[9][pixel] = input[pixel];

        color[pixel] = float4(0.f);
        for (int i = 0; i < 10; ++i)
            color[pixel] += LastFrames[i][pixel];
        color[pixel] /= 10.f;

        float x = (0.2126f * color[pixel].r + 0.7152f * color[pixel].g + 0.0722f * color[pixel].b) * 255.f;
        float threshold = 3.f;
        float f = (1.f / threshold) * log(threshold);
        float y = x <= threshold ? x * f : log(x);
        float rounding = 1e8f;
        y = round(y * rounding) / rounding;

        if (internalState[pixel] < y - 0.2f)
        {
            uint id = buffer_output.IncrementCounter();
            internalState[pixel] += 0.2f;
            output[pixel] = float4(1.f, 0.f, 0.f, 1.f);
            buffer_output[id] = uint2(gFrame, index * 2 + 1);
        }
        else if (internalState[pixel] > y + 0.2f)
        {
            uint id = buffer_output.IncrementCounter();
            internalState[pixel] -= 0.2f;
            output[pixel] = float4(0.f, 0.f, 1.f, 1.f);
            buffer_output[id] = uint2(gFrame, index * 2);
        }
        else
            output[pixel] = float4(1.f);
    }
}
