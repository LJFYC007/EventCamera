RWStructuredBuffer<half> input;
RWStructuredBuffer<float> vBuffer;
RWTexture2D<float4> output;
RWStructuredBuffer<uint2> buffer_output;

cbuffer PerFrameCB
{
    uint2 gResolution;
    uint gNetworkInputLength;
    uint gFrame;
    float gTau;
    float gThreshold;
}

[numthreads(32, 32, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    if (dispatchThreadID.x < gResolution.x && dispatchThreadID.y < gResolution.y)
    {
        uint2 tex_coord2 = uint2(dispatchThreadID.xy);
        uint index = dispatchThreadID.x * gResolution.y + dispatchThreadID.y;
        float x = input[index];
        float v = vBuffer[index];

        v = v * (1.f - 1.f / gTau) + x;
        float pos_spike = (v >= gThreshold) ? 1.f : 0.f;
        float neg_spike = (v <= -gThreshold) ? 1.f : 0.f;
        v = v - (pos_spike - neg_spike) * gThreshold;
        vBuffer[index] = v;
        float output_spike = pos_spike - neg_spike;

        index = dispatchThreadID.y * gResolution.x + dispatchThreadID.x;
        if (output_spike > 0.f)
        {
            uint id = buffer_output.IncrementCounter();
            buffer_output[id] = uint2(gFrame, index * 2 + 1);
            output[tex_coord2] = float4(1.f, 0.f, 0.f, 1.f);
        }
        else if (output_spike < 0.f)
        {
            uint id = buffer_output.IncrementCounter();
            buffer_output[id] = uint2(gFrame, index * 2);
            output[tex_coord2] = float4(0.f, 0.f, 1.f, 1.f);
        }
        else
            output[tex_coord2] = float4(1.f, 1.f, 1.f, 1.f);
    }
}
