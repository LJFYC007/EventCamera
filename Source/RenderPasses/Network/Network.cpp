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
#include <Utils/CudaRuntime.h>
#include <Utils/CudaUtils.h>
#include "Network.h"
#include <fstream>
#include <filesystem>
#include <chrono>

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, Network>();
}

namespace
{
const std::string kInputChannelEventImage = "input";
const std::string kOutputChannelEventImage = "output";
const std::string kAccumulatePass = "accumulatePass";
const std::string kONNXModelPath = "model_path";
} // namespace

void Network::prepareResources()
{
    if (mFrameDim.x == 0 || mFrameDim.y == 0)
        return;

    auto vbBindFlags = ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess | ResourceBindFlags::Shared;
    size_t type_size = sizeof(float);
    mpNetworkInputBuffer = mpDevice->createBuffer(type_size * mFrameDim.x * mFrameDim.y * networkInputLength, vbBindFlags);
    mpNetworkOutputBuffer = mpDevice->createBuffer(type_size * mFrameDim.x * mFrameDim.y * networkInputLength * 2, vbBindFlags);
    mpLastTexture = mpDevice->createTexture2D(
        mFrameDim.x, mFrameDim.y, ResourceFormat::R32Float, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
}

inline void checkCudaErrorCode(cudaError_t code)
{
    if (code != cudaSuccess)
    {
        std::string errMsg = "CUDA operation failed with code: " + std::to_string(code) + " (" + cudaGetErrorName(code) +
                             "), with message: " + cudaGetErrorString(code);
        throw std::runtime_error(errMsg);
    }
}

class MyLogger : public nvinfer1::ILogger
{
    void log(Severity severity, const char* msg) noexcept override
    {
        if (severity <= Severity::kWARNING)
        {
            Falcor::logInfo(msg);
        }
    }
} mylogger;

Network::Network(ref<Device> pDevice, const Properties& props) : RenderPass(pDevice) {
    std::string onnxModelPath = "";

    for (const auto& [key, value] : props)
    {
        if (key == kAccumulatePass)
            mAccumulatePass = value;
        else if (key == kONNXModelPath)
            onnxModelPath = props.get<std::string>(key);
        else
            logWarning("Unknown property '{}' in Network properties.", key);
    }
    prepareResources();

    FALCOR_CHECK(onnxModelPath != "", "no model specified!");
    // TensorRT
    auto builder = std::unique_ptr<nvinfer1::IBuilder>(nvinfer1::createInferBuilder(mylogger));
    if (builder.get() == nullptr)
        logFatal("builder is null");
    auto explicitBatch = 1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
    auto network = std::unique_ptr<nvinfer1::INetworkDefinition>(builder->createNetworkV2(explicitBatch));
    if (network.get() == nullptr)
        logFatal("network is null");
    auto parser = std::unique_ptr<nvonnxparser::IParser>(nvonnxparser::createParser(*network, mylogger));
    assert(parser.get() != nullptr);

    std::ifstream file(onnxModelPath, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size))
    {
        auto msg = "Error, unable to read model file";
        logError(msg);
        throw std::runtime_error(msg);
    }

    // Parse the buffer we read into memory.
    auto parsed = parser->parse(buffer.data(), buffer.size());
    for (int32_t i = 0; i < parser->getNbErrors(); ++i)
        logError("parser error:", parser->getError(i)->desc());
    if (!parsed)
    {
        auto msg = "Error, unable to parse model file";
        logError(msg);
        throw std::runtime_error(msg);
    }

    auto config = std::unique_ptr<nvinfer1::IBuilderConfig>(builder->createBuilderConfig());
    if (config == nullptr)
        logFatal("config is null");

    FALCOR_CHECK(network->getNbInputs() == 1, "input number should be 1");

    // Register a single optimization profile
    auto optProfile = builder->createOptimizationProfile();
    for (int i = 0; i < network->getNbInputs(); ++i)
    {
        auto inputName = network->getInput(i)->getName();
        mpInputNames.push_back(inputName);
        Falcor::logInfo("inputName {} is {}", i, inputName);
        int32_t inputB = 1024;
        int32_t inputC = 1;
        int32_t inputL = networkInputLength;
        const auto inputDim = nvinfer1::Dims3(inputB, inputC, inputL);
        optProfile->setDimensions(inputName, nvinfer1::OptProfileSelector::kMIN, inputDim);
        optProfile->setDimensions(inputName, nvinfer1::OptProfileSelector::kOPT, inputDim);
        optProfile->setDimensions(inputName, nvinfer1::OptProfileSelector::kMAX, inputDim);
    }
    config->addOptimizationProfile(optProfile);
    config->setBuilderOptimizationLevel(5);

    config->setFlag(nvinfer1::BuilderFlag::kTF32);
    Falcor::logInfo("Use FP32");

    checkCudaErrorCode(cudaStreamCreate(&mpStream));
    config->setProfileStream(mpStream);

    // Build the engine
    // If this call fails, it is suggested to increase the logger verbosity to
    // kVERBOSE and try rebuilding the engine. Doing so will provide you with more
    // information on why exactly it is failing.
    std::unique_ptr<nvinfer1::IHostMemory> plan{builder->buildSerializedNetwork(*network, *config)};
    if (plan.get() == nullptr)
        logFatal("plan is null");

    std::filesystem::path path(onnxModelPath);
    std::vector<std::filesystem::path> parts;
    for (const auto& part : path)
        parts.push_back(part);
    std::string model_description = parts[parts.size() - 2].string() + parts[parts.size() - 1].string() + ".plan";

    // Write the engine to disk
    const auto enginePath = parts[0] / parts[1] / model_description;
    std::ofstream outfile(enginePath, std::ofstream::binary);
    outfile.write(reinterpret_cast<const char*>(plan->data()), plan->size());
    Falcor::logInfo("Create plan Success, saved engine to {}", enginePath.string());

    // TensorRT Runtime
    mpRuntime = std::unique_ptr<nvinfer1::IRuntime>(nvinfer1::createInferRuntime(mylogger));
    if (mpRuntime.get() == nullptr)
        logFatal("runtime is null");
    auto ret = cudaSetDevice(0);
    if (ret != 0)
    {
        int numGPUs;
        cudaGetDeviceCount(&numGPUs);
        auto errMsg = "Unable to set GPU device index to: " + std::to_string(0) + ". Note, your device has " +
                      std::to_string(numGPUs) + " CUDA-capable GPU(s).";
        logError(errMsg);
        throw std::runtime_error(errMsg);
    }
    mpEngine = std::unique_ptr<nvinfer1::ICudaEngine>(mpRuntime->deserializeCudaEngine(plan->data(), plan->size()));
    if (mpEngine.get() == nullptr)
        logFatal("engine is null");

    mpContext = std::unique_ptr<nvinfer1::IExecutionContext>(mpEngine->createExecutionContext());
    if (mpContext.get() == nullptr)
        logFatal("context is null");

    FALCOR_CHECK(1 == network->getNbOutputs(), "output tensor number mismatch!");
    auto name = mpEngine->getIOTensorName(mpEngine->getNbIOTensors() - 1);
    Falcor::logInfo("output tensor name {} is {}", 0, name);
    mpOutputNames.push_back(name);
}

Properties Network::getProperties() const
{
    Properties props;
    props[kAccumulatePass] = mAccumulatePass;
    return props;
}

RenderPassReflection Network::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    reflector.addInput(kInputChannelEventImage, "Input accumulate image").bindFlags(ResourceBindFlags::ShaderResource);
    reflector.addOutput(kOutputChannelEventImage, "Output event image")
        .bindFlags(ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource)
        .format(ResourceFormat::RGBA32Float);
    return reflector;
}

void Network::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (mpScene == nullptr)
        return;
    auto start_time = std::chrono::high_resolution_clock::now();

    mAccumulateFrame++;
    if (mAccumulateFrame < mAccumulatePass)
        return;
    mAccumulateFrame = 0;
    mFrame++;

    // ----------------- Do the input pass -----------------
    if (!mpNetworkInputPass)
    {
        DefineList defines;
        mpScene->getShaderDefines(defines);
        ProgramDesc desc;
        mpScene->getShaderModules(desc.shaderModules);
        desc.addShaderLibrary("RenderPasses/Network/NetworkInput.cs.slang");
        desc.csEntry("main");
        mpScene->getTypeConformances(desc.typeConformances);
        mpNetworkInputPass = ComputePass::create(mpDevice, desc, defines);
    }

    ref<Texture> inputTexture = renderData.getTexture(kInputChannelEventImage);
    const uint2 resolution = uint2(inputTexture->getWidth(), inputTexture->getHeight());
    if (any(resolution != mFrameDim))
    {
        mFrameDim = resolution;
        prepareResources();
    }

    auto vars = mpNetworkInputPass->getRootVar();
    vars["input"] = inputTexture;
    vars["output"] = mpNetworkInputBuffer;
    vars["lastTexture"] = mpLastTexture;
    vars["PerFrameCB"]["gResolution"] = mFrameDim;
    vars["PerFrameCB"]["gNetworkInputLength"] = networkInputLength;
    mpNetworkInputPass->execute(pRenderContext, uint3(mFrameDim, 1));


    // ----------------- Do the inference -----------------
    int numPatches = 900; // mFrameDim.x * mFrameDim.y / 1024;
    int batchSize = 1024;
    float* base_input_ptr = static_cast<float*>(mpNetworkInputBuffer->getCudaMemory()->getMappedData());
    float* base_output_ptr = static_cast<float*>(mpNetworkOutputBuffer->getCudaMemory()->getMappedData());
    size_t type_size = sizeof(float);

    auto inference_start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numPatches; i++)
    {
        void* inputAddr = base_input_ptr + i * networkInputLength * batchSize;
        bool res = mpContext->setTensorAddress(mpInputNames[0].c_str(), inputAddr);
        if (!res)
            logFatal("Set input tensor {} address failed!", mpInputNames[0]);

        void* outputAddr = base_output_ptr + i * networkInputLength * batchSize * 2;
        res = mpContext->setTensorAddress(mpOutputNames[0].c_str(), outputAddr);
        if (!res)
            logFatal("Set output tensor address failed!");

        mpContext->enqueueV3(mpStream);
    }
    checkCudaErrorCode(cudaStreamSynchronize(mpStream));
    auto inference_end_time = std::chrono::high_resolution_clock::now();

    // ----------------- Do the output pass -----------------
    if (!mpNetworkOutputPass)
    {
        DefineList defines;
        mpScene->getShaderDefines(defines);
        ProgramDesc desc;
        mpScene->getShaderModules(desc.shaderModules);
        desc.addShaderLibrary("RenderPasses/Network/NetworkOutput.cs.slang");
        desc.csEntry("main");
        mpScene->getTypeConformances(desc.typeConformances);
        mpNetworkOutputPass = ComputePass::create(mpDevice, desc, defines);
    }

    ref<Texture> outputTexture = renderData.getTexture(kOutputChannelEventImage);
    vars = mpNetworkOutputPass->getRootVar();
    vars["input"] = mpNetworkOutputBuffer;
    vars["output"] = outputTexture;
    vars["PerFrameCB"]["gResolution"] = mFrameDim;
    vars["PerFrameCB"]["gNetworkInputLength"] = networkInputLength;
    mpNetworkOutputPass->execute(pRenderContext, uint3(mFrameDim, 1));

    auto end_time = std::chrono::high_resolution_clock::now();
    const auto inference_time_milli = 1000.0 * std::chrono::duration_cast<std::chrono::duration<double> >(inference_end_time - inference_start_time).count();
    const auto time_milli = 1000.0 * std::chrono::duration_cast<std::chrono::duration<double> >(end_time - start_time).count();
    Falcor::logInfo("Inference: {:.6f} ms, MyNetworkPass: {:.6f} ms", inference_time_milli, time_milli);
}

void Network::renderUI(Gui::Widgets& widget) {}

Network::~Network() { checkCudaErrorCode(cudaStreamDestroy(mpStream)); }
