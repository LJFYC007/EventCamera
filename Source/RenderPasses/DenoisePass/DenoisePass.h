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
#pragma once
#include "Falcor.h"
#include "RenderGraph/RenderPass.h"

using namespace Falcor;

class DenoisePass : public RenderPass
{
public:
    FALCOR_PLUGIN_CLASS(DenoisePass, "DenoisePass", "Insert pass description here.");

    static ref<DenoisePass> create(ref<Device> pDevice, const Properties& props)
    {
        return make_ref<DenoisePass>(pDevice, props);
    }

    DenoisePass(ref<Device> pDevice, const Properties& props);

    virtual Properties getProperties() const override;
    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void compile(RenderContext* pRenderContext, const CompileData& compileData) override {}
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual void renderUI(Gui::Widgets& widget) override;
    virtual void setScene(RenderContext* pRenderContext, const ref<Scene>& pScene) override { mpScene = pScene; }
    virtual bool onMouseEvent(const MouseEvent& mouseEvent) override { return false; }
    virtual bool onKeyEvent(const KeyboardEvent& keyEvent) override { return false; }

private:
    void prepareResources();

    /// Path to the directory where we store compressed data
    std::string mDirectoryPath;
    /// GPU buffer used for compression operations
    ref<Buffer> mpCompressBuffer;
    /// CPU-readable buffer for transferring compressed data from GPU
    ref<Buffer> mpReadbackBuffer;

    /// Compute pass that performs the denoise
    ref<ComputePass> mpDenoisePass;
    /// The current scene (or nullptr if no scene)
    ref<Scene> mpScene;

    /// Current frame dimension in pixels.
    uint2 mFrameDim = {0, 0};
    /// Number of current frame (didn't consider the accumulate pass)
    uint32_t mFrame = 0;
    /// Number of accumulate frame
    uint32_t mAccumulateFrame = 0;
    /// Accumulate Pass Frames
    uint32_t mAccumulatePass = 1;

    ref<Texture> mpLastFrames[10];
    ref<Texture> mpInternalState;
};
