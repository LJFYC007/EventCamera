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
#include "Falcor.h"
#include "Core/API/NativeHandleTraits.h"

#include "NRDPass.h"
#include "RenderPasses/Shared/Denoising/NRDConstants.slang"

namespace
{
const char kShaderPackRadiance[] = "RenderPasses/NRDPass/PackRadiance.cs.slang";

// Input buffer names.
const char kInputDiffuseRadianceHitDist[] = "diffuseRadianceHitDist";
const char kInputSpecularRadianceHitDist[] = "specularRadianceHitDist";
const char kInputSpecularHitDist[] = "specularHitDist";
const char kInputMotionVectors[] = "mvec";
const char kInputNormalRoughnessMaterialID[] = "normWRoughnessMaterialID";
const char kInputViewZ[] = "viewZ";
const char kInputDeltaPrimaryPosW[] = "deltaPrimaryPosW";
const char kInputDeltaSecondaryPosW[] = "deltaSecondaryPosW";

// Output buffer names.
const char kOutputFilteredDiffuseRadianceHitDist[] = "filteredDiffuseRadianceHitDist";
const char kOutputFilteredSpecularRadianceHitDist[] = "filteredSpecularRadianceHitDist";
const char kOutputReflectionMotionVectors[] = "reflectionMvec";
const char kOutputDeltaMotionVectors[] = "deltaMvec";

// Serialized parameters.

const char kEnabled[] = "enabled";
const char kMethod[] = "method";
const char kOutputSize[] = "outputSize";

// Common settings.
const char kWorldSpaceMotion[] = "worldSpaceMotion";
const char kDisocclusionThreshold[] = "disocclusionThreshold";

// Pack radiance settings.
const char kMaxIntensity[] = "maxIntensity";

// ReLAX diffuse/specular settings.
const char kDiffusePrepassBlurRadius[] = "diffusePrepassBlurRadius";
const char kSpecularPrepassBlurRadius[] = "specularPrepassBlurRadius";
const char kDiffuseMaxAccumulatedFrameNum[] = "diffuseMaxAccumulatedFrameNum";
const char kSpecularMaxAccumulatedFrameNum[] = "specularMaxAccumulatedFrameNum";
const char kDiffuseMaxFastAccumulatedFrameNum[] = "diffuseMaxFastAccumulatedFrameNum";
const char kSpecularMaxFastAccumulatedFrameNum[] = "specularMaxFastAccumulatedFrameNum";
const char kDiffusePhiLuminance[] = "diffusePhiLuminance";
const char kSpecularPhiLuminance[] = "specularPhiLuminance";
const char kDiffuseLobeAngleFraction[] = "diffuseLobeAngleFraction";
const char kSpecularLobeAngleFraction[] = "specularLobeAngleFraction";
const char kRoughnessFraction[] = "roughnessFraction";
const char kDiffuseHistoryRejectionNormalThreshold[] = "diffuseHistoryRejectionNormalThreshold";
const char kSpecularVarianceBoost[] = "specularVarianceBoost";
const char kSpecularLobeAngleSlack[] = "specularLobeAngleSlack";
const char kDisocclusionFixEdgeStoppingNormalPower[] = "disocclusionFixEdgeStoppingNormalPower";
const char kDisocclusionFixMaxRadius[] = "disocclusionFixMaxRadius";
const char kDisocclusionFixNumFramesToFix[] = "disocclusionFixNumFramesToFix";
const char kHistoryClampingColorBoxSigmaScale[] = "historyClampingColorBoxSigmaScale";
const char kSpatialVarianceEstimationHistoryThreshold[] = "spatialVarianceEstimationHistoryThreshold";
const char kAtrousIterationNum[] = "atrousIterationNum";
const char kMinLuminanceWeight[] = "minLuminanceWeight";
const char kDepthThreshold[] = "depthThreshold";
const char kRoughnessEdgeStoppingRelaxation[] = "roughnessEdgeStoppingRelaxation";
const char kNormalEdgeStoppingRelaxation[] = "normalEdgeStoppingRelaxation";
const char kLuminanceEdgeStoppingRelaxation[] = "luminanceEdgeStoppingRelaxation";
const char kEnableAntiFirefly[] = "enableAntiFirefly";
const char kEnableReprojectionTestSkippingWithoutMotion[] = "enableReprojectionTestSkippingWithoutMotion";
const char kEnableSpecularVirtualHistoryClamping[] = "enableSpecularVirtualHistoryClamping";
const char kEnableRoughnessEdgeStopping[] = "enableRoughnessEdgeStopping";
const char kEnableMaterialTestForDiffuse[] = "enableMaterialTestForDiffuse";
const char kEnableMaterialTestForSpecular[] = "enableMaterialTestForSpecular";

// Expose only togglable methods.
// There is no reason to expose runtime toggle for other methods.
const Gui::DropdownList kDenoisingMethod = {
    {(uint32_t)NRDPass::DenoisingMethod::RelaxDiffuseSpecular, "ReLAX"},
    {(uint32_t)NRDPass::DenoisingMethod::ReblurDiffuseSpecular, "ReBLUR"},
};
} // namespace

NRDPass::NRDPass(ref<Device> pDevice, const Properties& props) : RenderPass(pDevice)
{
    mpDevice->requireD3D12();

    DefineList definesRelax;
    definesRelax.add("NRD_USE_OCT_NORMAL_ENCODING", "1");
    definesRelax.add("NRD_USE_MATERIAL_ID", "0");
    definesRelax.add("NRD_METHOD", "0"); // NRD_METHOD_RELAX_DIFFUSE_SPECULAR
    mpPackRadiancePassRelax = ComputePass::create(mpDevice, kShaderPackRadiance, "main", definesRelax);

    DefineList definesReblur;
    definesReblur.add("NRD_USE_OCT_NORMAL_ENCODING", "1");
    definesReblur.add("NRD_USE_MATERIAL_ID", "0");
    definesReblur.add("NRD_METHOD", "1"); // NRD_METHOD_REBLUR_DIFFUSE_SPECULAR
    mpPackRadiancePassReblur = ComputePass::create(mpDevice, kShaderPackRadiance, "main", definesReblur);

    // Deserialize pass from dictionary.
    for (const auto& [key, value] : props)
    {
        if (key == kEnabled)
            mEnabled = value;
        else if (key == kMethod)
            mDenoisingMethod = value;
        else if (key == kOutputSize)
            mOutputSizeSelection = value;

        // Common settings.
        else if (key == kWorldSpaceMotion)
            mWorldSpaceMotion = value;
        else if (key == kDisocclusionThreshold)
            mDisocclusionThreshold = value;

        // Pack radiance settings.
        else if (key == kMaxIntensity)
            mMaxIntensity = value;

        // ReLAX diffuse/specular settings.
        else if (mDenoisingMethod == DenoisingMethod::RelaxDiffuseSpecular || mDenoisingMethod == DenoisingMethod::ReblurDiffuseSpecular)
        {
            logWarning("Unknown property '{}' in NRD properties.", key);
        }
        else if (mDenoisingMethod == DenoisingMethod::RelaxDiffuse)
        {
            logWarning("Unknown property '{}' in NRD properties.", key);
        }
        else
        {
            logWarning("Unknown property '{}' in NRD properties.", key);
        }
    }
}

Properties NRDPass::getProperties() const
{
    Properties props;

    props[kEnabled] = mEnabled;
    props[kMethod] = mDenoisingMethod;
    props[kOutputSize] = mOutputSizeSelection;

    // Common settings.
    props[kWorldSpaceMotion] = mWorldSpaceMotion;
    props[kDisocclusionThreshold] = mDisocclusionThreshold;

    // Pack radiance settings.
    props[kMaxIntensity] = mMaxIntensity;

    return props;
}

RenderPassReflection NRDPass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    const uint2 sz = RenderPassHelpers::calculateIOSize(mOutputSizeSelection, mScreenSize, compileData.defaultTexDims);

    if (mDenoisingMethod == DenoisingMethod::RelaxDiffuseSpecular || mDenoisingMethod == DenoisingMethod::ReblurDiffuseSpecular)
    {
        reflector.addInput(kInputDiffuseRadianceHitDist, "Diffuse radiance and hit distance");
        reflector.addInput(kInputSpecularRadianceHitDist, "Specular radiance and hit distance");
        reflector.addInput(kInputViewZ, "View Z");
        reflector.addInput(kInputNormalRoughnessMaterialID, "World normal, roughness, and material ID");
        reflector.addInput(kInputMotionVectors, "Motion vectors");

        reflector.addOutput(kOutputFilteredDiffuseRadianceHitDist, "Filtered diffuse radiance and hit distance")
            .format(ResourceFormat::RGBA16Float)
            .texture2D(sz.x, sz.y);
        reflector.addOutput(kOutputFilteredSpecularRadianceHitDist, "Filtered specular radiance and hit distance")
            .format(ResourceFormat::RGBA16Float)
            .texture2D(sz.x, sz.y);
    }
    else if (mDenoisingMethod == DenoisingMethod::RelaxDiffuse)
    {
        reflector.addInput(kInputDiffuseRadianceHitDist, "Diffuse radiance and hit distance");
        reflector.addInput(kInputViewZ, "View Z");
        reflector.addInput(kInputNormalRoughnessMaterialID, "World normal, roughness, and material ID");
        reflector.addInput(kInputMotionVectors, "Motion vectors");

        reflector.addOutput(kOutputFilteredDiffuseRadianceHitDist, "Filtered diffuse radiance and hit distance")
            .format(ResourceFormat::RGBA16Float)
            .texture2D(sz.x, sz.y);
    }
    else if (mDenoisingMethod == DenoisingMethod::SpecularReflectionMv)
    {
        reflector.addInput(kInputSpecularHitDist, "Specular hit distance");
        reflector.addInput(kInputViewZ, "View Z");
        reflector.addInput(kInputNormalRoughnessMaterialID, "World normal, roughness, and material ID");
        reflector.addInput(kInputMotionVectors, "Motion vectors");

        reflector.addOutput(kOutputReflectionMotionVectors, "Reflection motion vectors in screen space")
            .format(ResourceFormat::RG16Float)
            .texture2D(sz.x, sz.y);
    }
    else if (mDenoisingMethod == DenoisingMethod::SpecularDeltaMv)
    {
        reflector.addInput(kInputDeltaPrimaryPosW, "Delta primary world position");
        reflector.addInput(kInputDeltaSecondaryPosW, "Delta secondary world position");
        reflector.addInput(kInputMotionVectors, "Motion vectors");

        reflector.addOutput(kOutputDeltaMotionVectors, "Delta motion vectors in screen space")
            .format(ResourceFormat::RG16Float)
            .texture2D(sz.x, sz.y);
    }
    else
    {
        FALCOR_UNREACHABLE();
    }

    return reflector;
}

void NRDPass::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    mScreenSize = RenderPassHelpers::calculateIOSize(mOutputSizeSelection, mScreenSize, compileData.defaultTexDims);
    if (mScreenSize.x == 0 || mScreenSize.y == 0)
        mScreenSize = compileData.defaultTexDims;
    mFrameIndex = 0;
    reinit();
}

void NRDPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene)
        return;

    bool enabled = false;
    enabled = mEnabled;

    if (enabled)
    {
        executeInternal(pRenderContext, renderData);
    }
    else
    {
        if (mDenoisingMethod == DenoisingMethod::RelaxDiffuseSpecular || mDenoisingMethod == DenoisingMethod::ReblurDiffuseSpecular)
        {
            pRenderContext->blit(
                renderData.getTexture(kInputDiffuseRadianceHitDist)->getSRV(),
                renderData.getTexture(kOutputFilteredDiffuseRadianceHitDist)->getRTV()
            );
            pRenderContext->blit(
                renderData.getTexture(kInputSpecularRadianceHitDist)->getSRV(),
                renderData.getTexture(kOutputFilteredSpecularRadianceHitDist)->getRTV()
            );
        }
        else if (mDenoisingMethod == DenoisingMethod::RelaxDiffuse)
        {
            pRenderContext->blit(
                renderData.getTexture(kInputDiffuseRadianceHitDist)->getSRV(),
                renderData.getTexture(kOutputFilteredDiffuseRadianceHitDist)->getRTV()
            );
        }
        else if (mDenoisingMethod == DenoisingMethod::SpecularReflectionMv)
        {
            if (mWorldSpaceMotion)
            {
                pRenderContext->clearRtv(renderData.getTexture(kOutputReflectionMotionVectors)->getRTV().get(), float4(0.f));
            }
            else
            {
                pRenderContext->blit(
                    renderData.getTexture(kInputMotionVectors)->getSRV(), renderData.getTexture(kOutputReflectionMotionVectors)->getRTV()
                );
            }
        }
        else if (mDenoisingMethod == DenoisingMethod::SpecularDeltaMv)
        {
            if (mWorldSpaceMotion)
            {
                pRenderContext->clearRtv(renderData.getTexture(kOutputDeltaMotionVectors)->getRTV().get(), float4(0.f));
            }
            else
            {
                pRenderContext->blit(
                    renderData.getTexture(kInputMotionVectors)->getSRV(), renderData.getTexture(kOutputDeltaMotionVectors)->getRTV()
                );
            }
        }
    }
}

void NRDPass::renderUI(Gui::Widgets& widget)
{
    const nrd::LibraryDesc& nrdLibraryDesc = nrd::GetLibraryDesc();
    char name[256];
    _snprintf_s(name, 255, "NRD Library v%u.%u.%u", nrdLibraryDesc.versionMajor, nrdLibraryDesc.versionMinor, nrdLibraryDesc.versionBuild);
    widget.text(name);

    widget.checkbox("Enabled", mEnabled);

    if (mDenoisingMethod == DenoisingMethod::RelaxDiffuseSpecular || mDenoisingMethod == DenoisingMethod::ReblurDiffuseSpecular)
    {
        mRecreateDenoiser = widget.dropdown("Denoising method", kDenoisingMethod, reinterpret_cast<uint32_t&>(mDenoisingMethod));
    }
}

void NRDPass::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    mpScene = pScene;
}

static void* nrdAllocate(void* userArg, size_t size, size_t alignment)
{
    return malloc(size);
}

static void* nrdReallocate(void* userArg, void* memory, size_t size, size_t alignment)
{
    return realloc(memory, size);
}

static void nrdFree(void* userArg, void* memory)
{
    free(memory);
}

static ResourceFormat getFalcorFormat(nrd::Format format)
{
    switch (format)
    {
    case nrd::Format::R8_UNORM:
        return ResourceFormat::R8Unorm;
    case nrd::Format::R8_SNORM:
        return ResourceFormat::R8Snorm;
    case nrd::Format::R8_UINT:
        return ResourceFormat::R8Uint;
    case nrd::Format::R8_SINT:
        return ResourceFormat::R8Int;
    case nrd::Format::RG8_UNORM:
        return ResourceFormat::RG8Unorm;
    case nrd::Format::RG8_SNORM:
        return ResourceFormat::RG8Snorm;
    case nrd::Format::RG8_UINT:
        return ResourceFormat::RG8Uint;
    case nrd::Format::RG8_SINT:
        return ResourceFormat::RG8Int;
    case nrd::Format::RGBA8_UNORM:
        return ResourceFormat::RGBA8Unorm;
    case nrd::Format::RGBA8_SNORM:
        return ResourceFormat::RGBA8Snorm;
    case nrd::Format::RGBA8_UINT:
        return ResourceFormat::RGBA8Uint;
    case nrd::Format::RGBA8_SINT:
        return ResourceFormat::RGBA8Int;
    case nrd::Format::RGBA8_SRGB:
        return ResourceFormat::RGBA8UnormSrgb;
    case nrd::Format::R16_UNORM:
        return ResourceFormat::R16Unorm;
    case nrd::Format::R16_SNORM:
        return ResourceFormat::R16Snorm;
    case nrd::Format::R16_UINT:
        return ResourceFormat::R16Uint;
    case nrd::Format::R16_SINT:
        return ResourceFormat::R16Int;
    case nrd::Format::R16_SFLOAT:
        return ResourceFormat::R16Float;
    case nrd::Format::RG16_UNORM:
        return ResourceFormat::RG16Unorm;
    case nrd::Format::RG16_SNORM:
        return ResourceFormat::RG16Snorm;
    case nrd::Format::RG16_UINT:
        return ResourceFormat::RG16Uint;
    case nrd::Format::RG16_SINT:
        return ResourceFormat::RG16Int;
    case nrd::Format::RG16_SFLOAT:
        return ResourceFormat::RG16Float;
    case nrd::Format::RGBA16_UNORM:
        return ResourceFormat::RGBA16Unorm;
    case nrd::Format::RGBA16_SNORM:
        return ResourceFormat::Unknown; // Not defined in Falcor
    case nrd::Format::RGBA16_UINT:
        return ResourceFormat::RGBA16Uint;
    case nrd::Format::RGBA16_SINT:
        return ResourceFormat::RGBA16Int;
    case nrd::Format::RGBA16_SFLOAT:
        return ResourceFormat::RGBA16Float;
    case nrd::Format::R32_UINT:
        return ResourceFormat::R32Uint;
    case nrd::Format::R32_SINT:
        return ResourceFormat::R32Int;
    case nrd::Format::R32_SFLOAT:
        return ResourceFormat::R32Float;
    case nrd::Format::RG32_UINT:
        return ResourceFormat::RG32Uint;
    case nrd::Format::RG32_SINT:
        return ResourceFormat::RG32Int;
    case nrd::Format::RG32_SFLOAT:
        return ResourceFormat::RG32Float;
    case nrd::Format::RGB32_UINT:
        return ResourceFormat::RGB32Uint;
    case nrd::Format::RGB32_SINT:
        return ResourceFormat::RGB32Int;
    case nrd::Format::RGB32_SFLOAT:
        return ResourceFormat::RGB32Float;
    case nrd::Format::RGBA32_UINT:
        return ResourceFormat::RGBA32Uint;
    case nrd::Format::RGBA32_SINT:
        return ResourceFormat::RGBA32Int;
    case nrd::Format::RGBA32_SFLOAT:
        return ResourceFormat::RGBA32Float;
    case nrd::Format::R10_G10_B10_A2_UNORM:
        return ResourceFormat::RGB10A2Unorm;
    case nrd::Format::R10_G10_B10_A2_UINT:
        return ResourceFormat::RGB10A2Uint;
    case nrd::Format::R11_G11_B10_UFLOAT:
        return ResourceFormat::R11G11B10Float;
    case nrd::Format::R9_G9_B9_E5_UFLOAT:
        return ResourceFormat::RGB9E5Float;
    default:
        FALCOR_THROW("Unsupported NRD format.");
    }
}

static nrd::Method getNrdMethod(NRDPass::DenoisingMethod denoisingMethod)
{
    switch (denoisingMethod)
    {
    case NRDPass::DenoisingMethod::RelaxDiffuseSpecular:
        return nrd::Method::RELAX_DIFFUSE_SPECULAR;
    case NRDPass::DenoisingMethod::RelaxDiffuse:
        return nrd::Method::RELAX_DIFFUSE;
    case NRDPass::DenoisingMethod::ReblurDiffuseSpecular:
        return nrd::Method::REBLUR_DIFFUSE_SPECULAR;
    case NRDPass::DenoisingMethod::SpecularReflectionMv:
        return nrd::Method::SPECULAR_REFLECTION_MV;
    case NRDPass::DenoisingMethod::SpecularDeltaMv:
        return nrd::Method::SPECULAR_DELTA_MV;
    default:
        FALCOR_UNREACHABLE();
        return nrd::Method::RELAX_DIFFUSE_SPECULAR;
    }
}

/// Copies into col-major layout, as the NRD library works in column major layout,
/// while Falcor uses row-major layout
static void copyMatrix(float* dstMatrix, const float4x4& srcMatrix)
{
    float4x4 col_major = transpose(srcMatrix);
    memcpy(dstMatrix, static_cast<const float*>(col_major.data()), sizeof(float4x4));
}

void NRDPass::reinit()
{
    // Create a new denoiser instance.
    mpDenoiser = nullptr;

    const nrd::LibraryDesc& libraryDesc = nrd::GetLibraryDesc();

    const nrd::MethodDesc methods[] = {{getNrdMethod(mDenoisingMethod), uint16_t(mScreenSize.x), uint16_t(mScreenSize.y)}};

    nrd::DenoiserCreationDesc denoiserCreationDesc;
    denoiserCreationDesc.memoryAllocatorInterface.Allocate = nrdAllocate;
    denoiserCreationDesc.memoryAllocatorInterface.Reallocate = nrdReallocate;
    denoiserCreationDesc.memoryAllocatorInterface.Free = nrdFree;
    denoiserCreationDesc.requestedMethodNum = 1;
    denoiserCreationDesc.requestedMethods = methods;

    nrd::Result res = nrd::CreateDenoiser(denoiserCreationDesc, mpDenoiser);

    if (res != nrd::Result::SUCCESS)
        FALCOR_THROW("NRDPass: Failed to create NRD denoiser");

    createResources();
    createPipelines();
}

void NRDPass::createPipelines()
{
    mpPasses.clear();
    mpCachedProgramKernels.clear();
    mpCSOs.clear();
    mCBVSRVUAVdescriptorSetLayouts.clear();
    mpRootSignatures.clear();

    // Get denoiser desc for currently initialized denoiser implementation.
    const nrd::DenoiserDesc& denoiserDesc = nrd::GetDenoiserDesc(*mpDenoiser);

    // Create samplers descriptor layout and set.
    D3D12DescriptorSetLayout SamplersDescriptorSetLayout;

    for (uint32_t j = 0; j < denoiserDesc.staticSamplerNum; j++)
    {
        SamplersDescriptorSetLayout.addRange(ShaderResourceType::Sampler, denoiserDesc.staticSamplers[j].registerIndex, 1);
    }
    mpSamplersDescriptorSet =
        D3D12DescriptorSet::create(mpDevice, SamplersDescriptorSetLayout, D3D12DescriptorSetBindingUsage::ExplicitBind);

    // Set sampler descriptors right away.
    for (uint32_t j = 0; j < denoiserDesc.staticSamplerNum; j++)
    {
        mpSamplersDescriptorSet->setSampler(0, j, mpSamplers[j].get());
    }

    // Go over NRD passes and creating descriptor sets, root signatures and PSOs for each.
    for (uint32_t i = 0; i < denoiserDesc.pipelineNum; i++)
    {
        const nrd::PipelineDesc& nrdPipelineDesc = denoiserDesc.pipelines[i];
        const nrd::ComputeShader& nrdComputeShader = nrdPipelineDesc.computeShaderDXIL;

        // Initialize descriptor set.
        D3D12DescriptorSetLayout CBVSRVUAVdescriptorSetLayout;

        // Add constant buffer to descriptor set.
        CBVSRVUAVdescriptorSetLayout.addRange(ShaderResourceType::Cbv, denoiserDesc.constantBufferDesc.registerIndex, 1);

        for (uint32_t j = 0; j < nrdPipelineDesc.descriptorRangeNum; j++)
        {
            const nrd::DescriptorRangeDesc& nrdDescriptorRange = nrdPipelineDesc.descriptorRanges[j];

            ShaderResourceType descriptorType = nrdDescriptorRange.descriptorType == nrd::DescriptorType::TEXTURE
                                                    ? ShaderResourceType::TextureSrv
                                                    : ShaderResourceType::TextureUav;

            CBVSRVUAVdescriptorSetLayout.addRange(descriptorType, nrdDescriptorRange.baseRegisterIndex, nrdDescriptorRange.descriptorNum);
        }

        mCBVSRVUAVdescriptorSetLayouts.push_back(CBVSRVUAVdescriptorSetLayout);

        // Create root signature for the NRD pass.
        D3D12RootSignature::Desc rootSignatureDesc;
        rootSignatureDesc.addDescriptorSet(SamplersDescriptorSetLayout);
        rootSignatureDesc.addDescriptorSet(CBVSRVUAVdescriptorSetLayout);

        const D3D12RootSignature::Desc& desc = rootSignatureDesc;

        ref<D3D12RootSignature> pRootSig = D3D12RootSignature::create(mpDevice, desc);

        mpRootSignatures.push_back(pRootSig);

        // Create Compute PSO for the NRD pass.
        {
            std::string shaderFileName = "nrd/Shaders/Source/" + std::string(nrdPipelineDesc.shaderFileName) + ".hlsl";

            ProgramDesc programDesc;
            programDesc.addShaderLibrary(shaderFileName).csEntry(nrdPipelineDesc.shaderEntryPointName);
            programDesc.setCompilerFlags(SlangCompilerFlags::MatrixLayoutColumnMajor);
            // Disable warning 30056: non-short-circuiting `?:` operator is deprecated, use 'select' instead.
            programDesc.setCompilerArguments({"-Wno-30056"});
            DefineList defines;
            defines.add("NRD_COMPILER_DXC");
            defines.add("NRD_USE_OCT_NORMAL_ENCODING", "1");
            defines.add("NRD_USE_MATERIAL_ID", "0");
            ref<ComputePass> pPass = ComputePass::create(mpDevice, programDesc, defines);

            ref<Program> pProgram = pPass->getProgram();
            ref<const ProgramKernels> pProgramKernels = pProgram->getActiveVersion()->getKernels(mpDevice.get(), pPass->getVars().get());

            ComputeStateObjectDesc csoDesc;
            csoDesc.pProgramKernels = pProgramKernels;
            csoDesc.pD3D12RootSignatureOverride = pRootSig;

            ref<ComputeStateObject> pCSO = mpDevice->createComputeStateObject(csoDesc);

            mpPasses.push_back(pPass);
            mpCachedProgramKernels.push_back(pProgramKernels);
            mpCSOs.push_back(pCSO);
        }
    }
}

void NRDPass::createResources()
{
    // Destroy previously created resources.
    mpSamplers.clear();
    mpPermanentTextures.clear();
    mpTransientTextures.clear();

    const nrd::DenoiserDesc& denoiserDesc = nrd::GetDenoiserDesc(*mpDenoiser);
    const uint32_t poolSize = denoiserDesc.permanentPoolSize + denoiserDesc.transientPoolSize;

    // Create samplers.
    for (uint32_t i = 0; i < denoiserDesc.staticSamplerNum; i++)
    {
        const nrd::StaticSamplerDesc& nrdStaticsampler = denoiserDesc.staticSamplers[i];
        Sampler::Desc samplerDesc;
        samplerDesc.setFilterMode(TextureFilteringMode::Linear, TextureFilteringMode::Linear, TextureFilteringMode::Point);

        if (nrdStaticsampler.sampler == nrd::Sampler::NEAREST_CLAMP || nrdStaticsampler.sampler == nrd::Sampler::LINEAR_CLAMP)
        {
            samplerDesc.setAddressingMode(TextureAddressingMode::Clamp, TextureAddressingMode::Clamp, TextureAddressingMode::Clamp);
        }
        else
        {
            samplerDesc.setAddressingMode(TextureAddressingMode::Mirror, TextureAddressingMode::Mirror, TextureAddressingMode::Mirror);
        }

        if (nrdStaticsampler.sampler == nrd::Sampler::NEAREST_CLAMP || nrdStaticsampler.sampler == nrd::Sampler::NEAREST_MIRRORED_REPEAT)
        {
            samplerDesc.setFilterMode(TextureFilteringMode::Point, TextureFilteringMode::Point, TextureFilteringMode::Point);
        }
        else
        {
            samplerDesc.setFilterMode(TextureFilteringMode::Linear, TextureFilteringMode::Linear, TextureFilteringMode::Point);
        }

        mpSamplers.push_back(mpDevice->createSampler(samplerDesc));
    }

    // Texture pool.
    for (uint32_t i = 0; i < poolSize; i++)
    {
        const bool isPermanent = (i < denoiserDesc.permanentPoolSize);

        // Get texture desc.
        const nrd::TextureDesc& nrdTextureDesc =
            isPermanent ? denoiserDesc.permanentPool[i] : denoiserDesc.transientPool[i - denoiserDesc.permanentPoolSize];

        // Create texture.
        ResourceFormat textureFormat = getFalcorFormat(nrdTextureDesc.format);
        ref<Texture> pTexture = mpDevice->createTexture2D(
            nrdTextureDesc.width,
            nrdTextureDesc.height,
            textureFormat,
            1u,
            nrdTextureDesc.mipNum,
            nullptr,
            ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
        );

        if (isPermanent)
            mpPermanentTextures.push_back(pTexture);
        else
            mpTransientTextures.push_back(pTexture);
    }
}

void NRDPass::executeInternal(RenderContext* pRenderContext, const RenderData& renderData)
{
    FALCOR_ASSERT(mpScene);

    if (mRecreateDenoiser)
    {
        reinit();
    }

    if (mDenoisingMethod == DenoisingMethod::RelaxDiffuseSpecular)
    {
        // Run classic Falcor compute pass to pack radiance.
        {
            FALCOR_PROFILE(pRenderContext, "PackRadiance");
            auto perImageCB = mpPackRadiancePassRelax->getRootVar()["PerImageCB"];

            perImageCB["gMaxIntensity"] = mMaxIntensity;
            perImageCB["gDiffuseRadianceHitDist"] = renderData.getTexture(kInputDiffuseRadianceHitDist);
            perImageCB["gSpecularRadianceHitDist"] = renderData.getTexture(kInputSpecularRadianceHitDist);
            mpPackRadiancePassRelax->execute(pRenderContext, uint3(mScreenSize.x, mScreenSize.y, 1u));
        }

        nrd::SetMethodSettings(*mpDenoiser, nrd::Method::RELAX_DIFFUSE_SPECULAR, static_cast<void*>(&mRelaxDiffuseSpecularSettings));
    }
    else if (mDenoisingMethod == DenoisingMethod::RelaxDiffuse)
    {
        // Run classic Falcor compute pass to pack radiance and hit distance.
        {
            FALCOR_PROFILE(pRenderContext, "PackRadianceHitDist");
            auto perImageCB = mpPackRadiancePassRelax->getRootVar()["PerImageCB"];

            perImageCB["gMaxIntensity"] = mMaxIntensity;
            perImageCB["gDiffuseRadianceHitDist"] = renderData.getTexture(kInputDiffuseRadianceHitDist);
            mpPackRadiancePassRelax->execute(pRenderContext, uint3(mScreenSize.x, mScreenSize.y, 1u));
        }

        nrd::SetMethodSettings(*mpDenoiser, nrd::Method::RELAX_DIFFUSE, static_cast<void*>(&mRelaxDiffuseSettings));
    }
    else if (mDenoisingMethod == DenoisingMethod::ReblurDiffuseSpecular)
    {
        // Run classic Falcor compute pass to pack radiance and hit distance.
        {
            FALCOR_PROFILE(pRenderContext, "PackRadianceHitDist");
            auto perImageCB = mpPackRadiancePassReblur->getRootVar()["PerImageCB"];

            perImageCB["gHitDistParams"].setBlob(mReblurSettings.hitDistanceParameters);
            perImageCB["gMaxIntensity"] = mMaxIntensity;
            perImageCB["gDiffuseRadianceHitDist"] = renderData.getTexture(kInputDiffuseRadianceHitDist);
            perImageCB["gSpecularRadianceHitDist"] = renderData.getTexture(kInputSpecularRadianceHitDist);
            perImageCB["gNormalRoughness"] = renderData.getTexture(kInputNormalRoughnessMaterialID);
            perImageCB["gViewZ"] = renderData.getTexture(kInputViewZ);
            mpPackRadiancePassReblur->execute(pRenderContext, uint3(mScreenSize.x, mScreenSize.y, 1u));
        }

        nrd::SetMethodSettings(*mpDenoiser, nrd::Method::REBLUR_DIFFUSE_SPECULAR, static_cast<void*>(&mReblurSettings));
    }
    else if (mDenoisingMethod == DenoisingMethod::SpecularReflectionMv)
    {
        nrd::SpecularReflectionMvSettings specularReflectionMvSettings;
        nrd::SetMethodSettings(*mpDenoiser, nrd::Method::SPECULAR_REFLECTION_MV, static_cast<void*>(&specularReflectionMvSettings));
    }
    else if (mDenoisingMethod == DenoisingMethod::SpecularDeltaMv)
    {
        nrd::SpecularDeltaMvSettings specularDeltaMvSettings;
        nrd::SetMethodSettings(*mpDenoiser, nrd::Method::SPECULAR_DELTA_MV, static_cast<void*>(&specularDeltaMvSettings));
    }
    else
    {
        FALCOR_UNREACHABLE();
        return;
    }

    // Initialize common settings.
    float4x4 viewMatrix = mpScene->getCamera()->getViewMatrix();
    float4x4 projMatrix = mpScene->getCamera()->getData().projMatNoJitter;
    if (mFrameIndex == 0)
    {
        mPrevViewMatrix = viewMatrix;
        mPrevProjMatrix = projMatrix;
    }

    copyMatrix(mCommonSettings.viewToClipMatrix, projMatrix);
    copyMatrix(mCommonSettings.viewToClipMatrixPrev, mPrevProjMatrix);
    copyMatrix(mCommonSettings.worldToViewMatrix, viewMatrix);
    copyMatrix(mCommonSettings.worldToViewMatrixPrev, mPrevViewMatrix);
    // NRD's convention for the jitter is: [-0.5; 0.5] sampleUv = pixelUv + cameraJitter
    mCommonSettings.cameraJitter[0] = -mpScene->getCamera()->getJitterX();
    mCommonSettings.cameraJitter[1] = mpScene->getCamera()->getJitterY();
    mCommonSettings.denoisingRange = kNRDDepthRange;
    mCommonSettings.disocclusionThreshold = mDisocclusionThreshold * 0.01f;
    mCommonSettings.frameIndex = mFrameIndex;
    mCommonSettings.isMotionVectorInWorldSpace = mWorldSpaceMotion;

    mPrevViewMatrix = viewMatrix;
    mPrevProjMatrix = projMatrix;
    mFrameIndex++;

    // Run NRD dispatches.
    const nrd::DispatchDesc* dispatchDescs = nullptr;
    uint32_t dispatchDescNum = 0;
    nrd::Result result = nrd::GetComputeDispatches(*mpDenoiser, mCommonSettings, dispatchDescs, dispatchDescNum);
    FALCOR_ASSERT(result == nrd::Result::SUCCESS);

    for (uint32_t i = 0; i < dispatchDescNum; i++)
    {
        const nrd::DispatchDesc& dispatchDesc = dispatchDescs[i];
        FALCOR_PROFILE(pRenderContext, dispatchDesc.name);
        dispatch(pRenderContext, renderData, dispatchDesc);
    }

    // Submit the existing command list and start a new one.
    pRenderContext->submit();
}

void NRDPass::dispatch(RenderContext* pRenderContext, const RenderData& renderData, const nrd::DispatchDesc& dispatchDesc)
{
    const nrd::DenoiserDesc& denoiserDesc = nrd::GetDenoiserDesc(*mpDenoiser);
    const nrd::PipelineDesc& pipelineDesc = denoiserDesc.pipelines[dispatchDesc.pipelineIndex];

    // Set root signature.
    mpRootSignatures[dispatchDesc.pipelineIndex]->bindForCompute(pRenderContext);

    // Upload constants.
    auto cbAllocation = mpDevice->getUploadHeap()->allocate(dispatchDesc.constantBufferDataSize, ResourceBindFlags::Constant);
    std::memcpy(cbAllocation.pData, dispatchDesc.constantBufferData, dispatchDesc.constantBufferDataSize);

    // Create descriptor set for the NRD pass.
    ref<D3D12DescriptorSet> CBVSRVUAVDescriptorSet = D3D12DescriptorSet::create(
        mpDevice, mCBVSRVUAVdescriptorSetLayouts[dispatchDesc.pipelineIndex], D3D12DescriptorSetBindingUsage::ExplicitBind
    );

    // Set CBV.
    mpCBV = D3D12ConstantBufferView::create(mpDevice, cbAllocation.getGpuAddress(), cbAllocation.size);
    CBVSRVUAVDescriptorSet->setCbv(0 /* NB: range #0 is CBV range */, denoiserDesc.constantBufferDesc.registerIndex, mpCBV.get());

    uint32_t resourceIndex = 0;
    for (uint32_t descriptorRangeIndex = 0; descriptorRangeIndex < pipelineDesc.descriptorRangeNum; descriptorRangeIndex++)
    {
        const nrd::DescriptorRangeDesc& nrdDescriptorRange = pipelineDesc.descriptorRanges[descriptorRangeIndex];

        for (uint32_t descriptorOffset = 0; descriptorOffset < nrdDescriptorRange.descriptorNum; descriptorOffset++)
        {
            FALCOR_ASSERT(resourceIndex < dispatchDesc.resourceNum);
            const nrd::Resource& resource = dispatchDesc.resources[resourceIndex];

            FALCOR_ASSERT(resource.stateNeeded == nrdDescriptorRange.descriptorType);

            ref<Texture> texture;

            switch (resource.type)
            {
            case nrd::ResourceType::IN_MV:
                texture = renderData.getTexture(kInputMotionVectors);
                break;
            case nrd::ResourceType::IN_NORMAL_ROUGHNESS:
                texture = renderData.getTexture(kInputNormalRoughnessMaterialID);
                break;
            case nrd::ResourceType::IN_VIEWZ:
                texture = renderData.getTexture(kInputViewZ);
                break;
            case nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST:
                texture = renderData.getTexture(kInputDiffuseRadianceHitDist);
                break;
            case nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST:
                texture = renderData.getTexture(kInputSpecularRadianceHitDist);
                break;
            case nrd::ResourceType::IN_SPEC_HITDIST:
                texture = renderData.getTexture(kInputSpecularHitDist);
                break;
            case nrd::ResourceType::IN_DELTA_PRIMARY_POS:
                texture = renderData.getTexture(kInputDeltaPrimaryPosW);
                break;
            case nrd::ResourceType::IN_DELTA_SECONDARY_POS:
                texture = renderData.getTexture(kInputDeltaSecondaryPosW);
                break;
            case nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST:
                texture = renderData.getTexture(kOutputFilteredDiffuseRadianceHitDist);
                break;
            case nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST:
                texture = renderData.getTexture(kOutputFilteredSpecularRadianceHitDist);
                break;
            case nrd::ResourceType::OUT_REFLECTION_MV:
                texture = renderData.getTexture(kOutputReflectionMotionVectors);
                break;
            case nrd::ResourceType::OUT_DELTA_MV:
                texture = renderData.getTexture(kOutputDeltaMotionVectors);
                break;
            case nrd::ResourceType::TRANSIENT_POOL:
                texture = mpTransientTextures[resource.indexInPool];
                break;
            case nrd::ResourceType::PERMANENT_POOL:
                texture = mpPermanentTextures[resource.indexInPool];
                break;
            default:
                FALCOR_ASSERT(!"Unavailable resource type");
                break;
            }

            FALCOR_ASSERT(texture);

            // Set up resource barriers.
            Resource::State newState =
                resource.stateNeeded == nrd::DescriptorType::TEXTURE ? Resource::State::ShaderResource : Resource::State::UnorderedAccess;
            for (uint16_t mip = 0; mip < resource.mipNum; mip++)
            {
                const ResourceViewInfo viewInfo = ResourceViewInfo(resource.mipOffset + mip, 1, 0, 1);
                pRenderContext->resourceBarrier(texture.get(), newState, &viewInfo);
            }

            // Set the SRV and UAV descriptors.
            if (nrdDescriptorRange.descriptorType == nrd::DescriptorType::TEXTURE)
            {
                ref<ShaderResourceView> pSRV = texture->getSRV(resource.mipOffset, resource.mipNum, 0, 1);
                CBVSRVUAVDescriptorSet->setSrv(
                    descriptorRangeIndex + 1 /* NB: range #0 is CBV range */,
                    nrdDescriptorRange.baseRegisterIndex + descriptorOffset,
                    pSRV.get()
                );
            }
            else
            {
                ref<UnorderedAccessView> pUAV = texture->getUAV(resource.mipOffset, 0, 1);
                CBVSRVUAVDescriptorSet->setUav(
                    descriptorRangeIndex + 1 /* NB: range #0 is CBV range */,
                    nrdDescriptorRange.baseRegisterIndex + descriptorOffset,
                    pUAV.get()
                );
            }

            resourceIndex++;
        }
    }

    FALCOR_ASSERT(resourceIndex == dispatchDesc.resourceNum);

    // Set descriptor sets.
    mpSamplersDescriptorSet->bindForCompute(pRenderContext, mpRootSignatures[dispatchDesc.pipelineIndex].get(), 0);
    CBVSRVUAVDescriptorSet->bindForCompute(pRenderContext, mpRootSignatures[dispatchDesc.pipelineIndex].get(), 1);

    // Set pipeline state.
    ref<ComputePass> pPass = mpPasses[dispatchDesc.pipelineIndex];
    ref<Program> pProgram = pPass->getProgram();
    ref<const ProgramKernels> pProgramKernels = pProgram->getActiveVersion()->getKernels(mpDevice.get(), pPass->getVars().get());

    // Check if anything changed.
    bool newProgram = (pProgramKernels.get() != mpCachedProgramKernels[dispatchDesc.pipelineIndex].get());
    if (newProgram)
    {
        mpCachedProgramKernels[dispatchDesc.pipelineIndex] = pProgramKernels;

        ComputeStateObjectDesc desc;
        desc.pProgramKernels = pProgramKernels;
        desc.pD3D12RootSignatureOverride = mpRootSignatures[dispatchDesc.pipelineIndex];

        ref<ComputeStateObject> pCSO = mpDevice->createComputeStateObject(desc);
        mpCSOs[dispatchDesc.pipelineIndex] = pCSO;
    }
    ID3D12GraphicsCommandList* pCommandList =
        pRenderContext->getLowLevelData()->getCommandBufferNativeHandle().as<ID3D12GraphicsCommandList*>();
    ID3D12PipelineState* pPipelineState = mpCSOs[dispatchDesc.pipelineIndex]->getNativeHandle().as<ID3D12PipelineState*>();

    pCommandList->SetPipelineState(pPipelineState);

    // Dispatch.
    pCommandList->Dispatch(dispatchDesc.gridWidth, dispatchDesc.gridHeight, 1);

    mpDevice->getUploadHeap()->release(cbAllocation);
}

extern "C" FALCOR_API_EXPORT void registerPlugin(PluginRegistry& registry)
{
    registry.registerClass<RenderPass, NRDPass>();
}
