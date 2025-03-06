/***************************************************************************
 # Copyright (c) 2015-23, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
Texture2D<float4> gReference;
Texture2D<float4> gSource;
Texture2D<float4> gWorldPosition;

cbuffer PerFrameCB
{
    uint2 gResolution;
    float gThreshold;
    uint gMethod;
    uint gNeedAccumulatedEvents;
    uint gToleranceEvents;
};

RWTexture2D<float4> gResult;
RWTexture2D<float> gLastEvent;
RWTexture2DArray<float> gRecentSum;
RWTexture2DArray<uint> gRecentCount;
RWTexture2D<uint> gCurrentIndex;

void clearRecentStatus(uint2 pixel)
{
    for (int i = 0; i < 5; ++i)
    {
        gRecentSum[uint3(pixel, i)] = 0.f;
        gRecentCount[uint3(pixel, i)] = 0;
    }
}

void missEvent(uint2 pixel)
{
    for (int i = 3; i >= 0; --i)
    {
        gRecentSum[uint3(pixel, i + 1)] = gRecentSum[uint3(pixel, i)];
        gRecentCount[uint3(pixel, i + 1)] = gRecentCount[uint3(pixel, i)];
    }
    gRecentSum[uint3(pixel, 0)] = 0.f;
    gRecentCount[uint3(pixel, 0)] = 0;
}

void updateRecentStatus(uint2 pixel, float illuminance)
{
    for (int i = 0; i < 5; ++i)
    {
        gRecentSum[uint3(pixel, i)] += illuminance;
        gRecentCount[uint3(pixel, i)]++;
    }
}

[numthreads(16, 16, 1)]
void main(uint3 DTid: SV_DispatchThreadID)
{
    const uint2 pixel = DTid.xy;
    if (any(pixel >= gResolution))
        return;

    gResult[pixel] = float4(0.f);
    float nowI = 0.299f * gSource[pixel].r + 0.587f * gSource[pixel].g + 0.114f * gSource[pixel].b;
    float preI = gLastEvent[pixel];
    updateRecentStatus(pixel, nowI);

    float diff = log10(nowI / preI);
    bool type;
    if (diff > gThreshold) type = false;
    else if (diff < -gThreshold) type = true;
    else
    {
        missEvent(pixel);
        return;
    }

    bool lastType = gRecentSum[uint3(pixel, 0)] > preI * gRecentCount[uint3(pixel, 0)] ? false : true;
    if (lastType != type)
    {
        missEvent(pixel);
        return;
    }

    if (gRecentCount[uint3(pixel, gToleranceEvents)] == gNeedAccumulatedEvents)
    {
        gLastEvent[pixel] = gRecentSum[uint3(pixel, gToleranceEvents)] / gNeedAccumulatedEvents;
        clearRecentStatus(pixel);
        gResult[pixel] = type ? float4(0.f, 1.f, 0.f, 0.f) : float4(1.f, 0.f, 0.f, 0.f);
    }
}
