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
#include "BlockStoragePass.h"
#include <fstream>

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, BlockStoragePass>();
}

namespace
{
const std::string kInputChannelEventImage = "input";
const std::string kOutputChannelEventImage = "output";
const std::string kEnabled = "enabled";
const std::string kDirectory = "directory";
} // namespace

void BlockStoragePass::prepareResources()
{
    if (mFrameDim.x == 0 || mFrameDim.y == 0)
        return;

    // Create a 2D texture array with 100 slices to store frames
    mpStorageTexture = mpDevice->createTexture2D(
        mFrameDim.x,
        mFrameDim.y,
        ResourceFormat::RGBA32Float,
        frameCapacity, // arraySize - storing multiple frames
        1,             // mipLevels
        nullptr,       // pInitData
        ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
    );
}

BlockStoragePass::BlockStoragePass(ref<Device> pDevice, const Properties& props) : RenderPass(pDevice)
{
    for (const auto& [key, value] : props)
    {
        if (key == kEnabled)
            mEnabled = value;
        if (key == kDirectory)
            mDirectoryPath = props.get<std::string>(key);
        else
            logWarning("Unknown property '{}' in BlockStoragePass properties.", key);
    }

    prepareResources();
}

Properties BlockStoragePass::getProperties() const
{
    Properties props;
    props[kEnabled] = mEnabled;
    props[kDirectory] = mDirectoryPath;
    return props;
}

RenderPassReflection BlockStoragePass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    reflector.addInput(kInputChannelEventImage, "Event image").bindFlags(ResourceBindFlags::ShaderResource);
    reflector.addOutput(kOutputChannelEventImage, "Empty buffer")
        .bindFlags(ResourceBindFlags::UnorderedAccess)
        .format(ResourceFormat::RGBA32Float);
    return reflector;
}
void BlockStoragePass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (mpScene == nullptr)
        return;

    if (!mpComputePass)
    {
        DefineList defines;
        mpScene->getShaderDefines(defines);
        ProgramDesc desc;
        mpScene->getShaderModules(desc.shaderModules);
        desc.addShaderLibrary("RenderPasses/BlockStoragePass/BlockStoragePass.cs.slang");
        desc.csEntry("main");
        mpScene->getTypeConformances(desc.typeConformances);
        mpComputePass = ComputePass::create(mpDevice, desc, defines);
    }

    if (!mEnabled)
        return;

    ref<Texture> inputTexture = renderData.getTexture(kInputChannelEventImage);
    const uint2 resolution = uint2(inputTexture->getWidth(), inputTexture->getHeight());
    if (any(resolution != mFrameDim))
    {
        mFrameDim = resolution;
        prepareResources();
    }

    auto vars = mpComputePass->getRootVar();
    vars["input"] = inputTexture;
    vars["output"] = mpStorageTexture;
    vars["PerFrameCB"]["gResolution"] = mFrameDim;
    vars["PerFrameCB"]["frame_id"] = mFrame % frameCapacity;

    mpComputePass->execute(pRenderContext, uint3(mFrameDim, 1));

    mFrame++;

    // When we reach capacity, write to file in 64x64x64 blocks
    if (mFrame % frameCapacity == 0 && !mDirectoryPath.empty())
    {
        std::vector<uint8_t> whole_buffer(mFrameDim.x * mFrameDim.y * frameCapacity * sizeof(float) * 4);

        // Write the texture data to the file
        for (uint32_t i = 0; i < frameCapacity; i++)
        {
            auto data = pRenderContext->readTextureSubresource(mpStorageTexture.get(), mpStorageTexture->getSubresourceIndex(i, 0));
            memcpy(whole_buffer.data() + i * mFrameDim.x * mFrameDim.y * sizeof(float) * 4, data.data(), data.size());
        }

        // Calculate number of blocks in each dimension
        uint32_t blocksX = (mFrameDim.x + 63) / 64;
        uint32_t blocksY = (mFrameDim.y + 63) / 64;
        uint32_t blockZ = mFrame / frameCapacity - 1; // Which group of frames this is (0-based)

        for (uint32_t by = 0; by < blocksY; by++)
        {
            for (uint32_t bx = 0; bx < blocksX; bx++)
            {
                // Create a buffer for this block
                std::vector<uint8_t> blockBuffer(64 * 64 * 64 * sizeof(float) * 4, 0);

                // Fill the block buffer
                for (uint32_t z = 0; z < 64; z++) // z now iterates through the frames in this batch
                {
                    for (uint32_t y = 0; y < 64 && (by * 64 + y) < mFrameDim.y; y++)
                    {
                        for (uint32_t x = 0; x < 64 && (bx * 64 + x) < mFrameDim.x; x++)
                        {
                            uint32_t srcIdx = (z * mFrameDim.y + (by * 64 + y)) * mFrameDim.x + (bx * 64 + x);
                            uint32_t dstIdx = (z * 64 + y) * 64 + x;

                            // Copy RGBA (4 floats)
                            memcpy(
                                blockBuffer.data() + dstIdx * sizeof(float) * 4,
                                whole_buffer.data() + srcIdx * sizeof(float) * 4,
                                sizeof(float) * 4
                            );
                        }
                    }
                }

                // Write this block to file
                std::string filename =
                    mDirectoryPath + "/block_" + std::to_string(bx) + "_" + std::to_string(by) + "_" + std::to_string(blockZ) + ".bin";

                std::ofstream file(filename, std::ios::binary);
                if (file.is_open())
                {
                    file.write(reinterpret_cast<char*>(blockBuffer.data()), blockBuffer.size());
                    file.close();
                }
                else
                {
                    logError("Failed to write block to file: {}", filename);
                }
            }
        }
    }
}

void BlockStoragePass::renderUI(Gui::Widgets& widget)
{
    widget.checkbox("Enabled", mEnabled);
}
