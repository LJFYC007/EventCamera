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
#include "CompressPass.h"
#include <fstream>

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, CompressPass>();
}

namespace
{
const std::string kInputChannelEventImage = "input";
const std::string kOutputChannelEventImage = "output";
} // namespace

CompressPass::CompressPass(ref<Device> pDevice, const Properties& props) : RenderPass(pDevice)
{
    auto vbBindFlags = ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess | ResourceBindFlags::Shared;
    size_t type_size = sizeof(uint4); // Align to 16 bytes
    mpCompressBuffer = mpDevice->createStructuredBuffer(type_size, 1280 * 720, vbBindFlags, MemoryType::DeviceLocal, nullptr, true);

    mpReadbackBuffer = pDevice->createBuffer(mpCompressBuffer->getSize(), ResourceBindFlags::None, MemoryType::ReadBack);
}

Properties CompressPass::getProperties() const
{
    return {};
}

RenderPassReflection CompressPass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    reflector.addInput(kInputChannelEventImage, "Event image").bindFlags(ResourceBindFlags::ShaderResource);
    reflector.addOutput(kOutputChannelEventImage, "Empty buffer")
        .bindFlags(ResourceBindFlags::UnorderedAccess)
        .format(ResourceFormat::RGBA32Float);
    return reflector;
}

void CompressPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (mpScene == nullptr)
        return;

    if (!mpComputePass)
    {
        DefineList defines;
        mpScene->getShaderDefines(defines);
        ProgramDesc desc;
        mpScene->getShaderModules(desc.shaderModules);
        desc.addShaderLibrary("RenderPasses/CompressPass/BufferPass.cs.slang");
        desc.csEntry("main");
        mpScene->getTypeConformances(desc.typeConformances);
        mpComputePass = ComputePass::create(mpDevice, desc, defines);
    }

    auto vars = mpComputePass->getRootVar();
    pRenderContext->clearUAVCounter(mpCompressBuffer, 0);
    vars["input"] = renderData.getTexture(kInputChannelEventImage);
    vars["output"] = renderData.getTexture(kOutputChannelEventImage);
    vars["buffer_output"] = mpCompressBuffer;

    mpComputePass->execute(pRenderContext, uint3(1280, 720, 1));

    ref<Buffer> pCounterBuffer = mpCompressBuffer->getUAVCounter();
    pRenderContext->copyBufferRegion(mpReadbackBuffer.get(), 0, pCounterBuffer.get(), 0, pCounterBuffer->getSize());
    mpDevice->wait();
    const uint32_t pCounterValue = *static_cast<const uint32_t*>(mpReadbackBuffer->map());
    const uint32_t pDataSize = pCounterValue* sizeof(uint4);
    mpReadbackBuffer->unmap();

    // Map the CPU buffer and write to file
    pRenderContext->copyBufferRegion(mpReadbackBuffer.get(), 0, mpCompressBuffer.get(), 0, pDataSize);
    mpDevice->wait();
    const uint4* data = reinterpret_cast<const uint4*>(mpReadbackBuffer->map());
    std::string filename = "C:\\Users\\-LJF007-\\Documents\\output\\output.bin";
    std::ofstream file(filename, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data), pDataSize);
    file.close();
    mpReadbackBuffer->unmap();
}

void CompressPass::renderUI(Gui::Widgets& widget) {}
