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
#include "DenoisePass.h"
#include <fstream>
#include <Utils/CudaRuntime.h>
#include <Utils/CudaUtils.h>
#include <filesystem>
#include <chrono>

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, DenoisePass>();
}

namespace
{
const std::string kInputChannelEventImage = "input";
const std::string kColorChannelEventImage = "color";
const std::string kOutputChannelEventImage = "output";
const std::string kAccumulatePass = "accumulatePass";
const std::string kDirectory = "directory";
const std::string kWindow = "window";
} // namespace

DenoisePass::DenoisePass(ref<Device> pDevice, const Properties& props) : RenderPass(pDevice)
{
    for (const auto& [key, value] : props)
    {
        if (key == kAccumulatePass)
            mAccumulatePass = value;
        else if (key == kWindow)
            mWindowSize = value;
        else if (key == kDirectory)
            mDirectoryPath = props.get<std::string>(key);
        else
            logWarning("Unknown property '{}' in Denoise properties.", key);
    }
}

Properties DenoisePass::getProperties() const
{
    Properties props;
    props[kAccumulatePass] = mAccumulatePass;
    props[kWindow] = mWindowSize;
    props[kDirectory] = mDirectoryPath;
    return props;
}

RenderPassReflection DenoisePass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    reflector.addInput(kInputChannelEventImage, "Input accumulate image").bindFlags(ResourceBindFlags::ShaderResource);
    reflector.addOutput(kColorChannelEventImage, "Output color image")
        .bindFlags(ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource)
        .format(ResourceFormat::RGBA32Float);
    reflector.addOutput(kOutputChannelEventImage, "Output event image")
        .bindFlags(ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource)
        .format(ResourceFormat::RGBA32Float);
    return reflector;
}

void DenoisePass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (mpScene == nullptr)
        return;

    mAccumulateFrame++;
    if (mAccumulateFrame < mAccumulatePass)
        return;
    mAccumulateFrame = 0;
    mFrame++;

    // -------------------- Do the denoise pass --------------------
    if (!mpDenoisePass)
    {
        DefineList defines;
        mpScene->getShaderDefines(defines);
        ProgramDesc desc;
        mpScene->getShaderModules(desc.shaderModules);
        desc.addShaderLibrary("RenderPasses/DenoisePass/Denoise.slang");
        desc.csEntry("main");
        mpScene->getTypeConformances(desc.typeConformances);
        mpDenoisePass = ComputePass::create(mpDevice, desc, defines);
    }

    ref<Texture> inputTexture = renderData.getTexture(kInputChannelEventImage);
    const uint2 resolution = uint2(inputTexture->getWidth(), inputTexture->getHeight());
    if (any(resolution != mFrameDim))
    {
        mFrameDim = resolution;
        prepareResources();
    }

    auto vars = mpDenoisePass->getRootVar();
    vars["input"] = inputTexture;
    vars["color"] = renderData.getTexture(kColorChannelEventImage);
    vars["output"] = renderData.getTexture(kOutputChannelEventImage);
    vars["PerFrameCB"]["gResolution"] = mFrameDim;
    vars["PerFrameCB"]["gFrame"] = mFrame - mWindowSize / 2;
    vars["PerFrameCB"]["gWindow"] = mWindowSize;
    for (int i = 0; i < 10; ++ i)
        vars["LastFrames"][i] = mpLastFrames[i];
    pRenderContext->clearUAVCounter(mpCompressBuffer, 0);
    vars["buffer_output"] = mpCompressBuffer;
    vars["internalState"] = mpInternalState;
    mpDenoisePass->execute(pRenderContext, uint3(mFrameDim, 1));

    // -------------------- Read back the compressed data --------------------
    if (mFrame >= 10)
    {
        ref<Buffer> pCounterBuffer = mpCompressBuffer->getUAVCounter();
        pRenderContext->copyBufferRegion(mpReadbackBuffer.get(), 0, pCounterBuffer.get(), 0, pCounterBuffer->getSize());
        mpDevice->wait();
        const uint32_t pCounterValue = *static_cast<const uint32_t*>(mpReadbackBuffer->map());
        const uint32_t pDataSize = pCounterValue * sizeof(uint2);
        mpReadbackBuffer->unmap();

        pRenderContext->copyBufferRegion(mpReadbackBuffer.get(), 0, mpCompressBuffer.get(), 0, pDataSize);
        mpDevice->wait();
        const uint* data = reinterpret_cast<const uint*>(mpReadbackBuffer->map());
        std::string filename = mDirectoryPath + "\\data-" + std::to_string(mFrame) + ".bin";
        std::ofstream file(filename, std::ios::binary);
        file.write(reinterpret_cast<const char*>(data), pDataSize);
        file.close();
        mpReadbackBuffer->unmap();
    }
}

void DenoisePass::renderUI(Gui::Widgets& widget) {}

void DenoisePass::prepareResources()
{
    mpInternalState = mpDevice->createTexture2D(
        mFrameDim.x, mFrameDim.y, ResourceFormat::R32Float, 1, 1, nullptr, ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource
    );

    for (int i = 0; i < 10; ++ i)
        mpLastFrames[i] = mpDevice->createTexture2D(
            mFrameDim.x, mFrameDim.y, ResourceFormat::RGBA32Float, 1, 1, nullptr, ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource
        );

    size_t type_size = sizeof(uint2);
    auto vbBindFlags = ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess | ResourceBindFlags::Shared;
    mpCompressBuffer = mpDevice->createStructuredBuffer(type_size, mFrameDim.x * mFrameDim.y, vbBindFlags, MemoryType::DeviceLocal, nullptr, true);
    mpReadbackBuffer = mpDevice->createBuffer(mpCompressBuffer->getSize(), ResourceBindFlags::None, MemoryType::ReadBack);
}
