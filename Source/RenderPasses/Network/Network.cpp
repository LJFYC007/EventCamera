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
const std::string kDirectory = "directory";
const std::string kNetworkInputLength = "networkInputLength";
const std::string kBatchSize = "batchSize";
} // namespace

void Network::prepareResources()
{
    if (mFrameDim.x == 0 || mFrameDim.y == 0)
        return;

    auto vbBindFlags = ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess | ResourceBindFlags::Shared;
    size_t type_size = sizeof(float) / 2;
    size_t storage = (mFrameDim.x * mFrameDim.y / batchSize + 1) * batchSize * networkInputLength * type_size;
    mpNetworkInputBuffer = mpDevice->createBuffer(storage, vbBindFlags);
    mpNetworkOutputBuffer = mpDevice->createBuffer(storage * 2, vbBindFlags);
    mpLastTexture = mpDevice->createTexture2D(
        mFrameDim.x, mFrameDim.y, ResourceFormat::R32Float, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);

    type_size = sizeof(uint2);
    mpCompressBuffer = mpDevice->createStructuredBuffer(type_size, mFrameDim.x * mFrameDim.y, vbBindFlags, MemoryType::DeviceLocal, nullptr, true);
    mpReadbackBuffer = mpDevice->createBuffer(mpCompressBuffer->getSize(), ResourceBindFlags::None, MemoryType::ReadBack);
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
        else if (key == kDirectory)
            mDirectoryPath = props.get<std::string>(key);
        else if (key == kNetworkInputLength)
            networkInputLength = value;
        else if (key == kBatchSize)
            batchSize = value;
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
        int32_t inputB = batchSize;
        int32_t inputC = 1;
        int32_t inputL = networkInputLength;
        const auto inputDim = nvinfer1::Dims3(inputB, inputC, inputL);
        optProfile->setDimensions(inputName, nvinfer1::OptProfileSelector::kMIN, inputDim);
        optProfile->setDimensions(inputName, nvinfer1::OptProfileSelector::kOPT, inputDim);
        optProfile->setDimensions(inputName, nvinfer1::OptProfileSelector::kMAX, inputDim);
    }
    config->addOptimizationProfile(optProfile);
    config->setBuilderOptimizationLevel(5);

    /*
    config->setFlag(nvinfer1::BuilderFlag::kTF32);
    Falcor::logInfo("Use FP32");
    */
    if (!builder->platformHasFastFp16())
    {
        auto msg = "Error: GPU does not support FP16 precision";
        logError(msg);
        throw std::runtime_error(msg);
    }
    config->setFlag(nvinfer1::BuilderFlag::kFP16);
    for (int i = 0; i < network->getNbInputs(); ++i)
        network->getInput(i)->setType(nvinfer1::DataType::kHALF);
    for (int i = 0; i < network->getNbOutputs(); ++i)
        network->getOutput(i)->setType(nvinfer1::DataType::kHALF);
    Falcor::logInfo("Use FP16");

    mpStream.resize(streamCnt);
    for (uint i = 0; i < streamCnt; ++i)
    {
        checkCudaErrorCode(cudaStreamCreate(&mpStream[i]));
        config->setProfileStream(mpStream[i]);
    }

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

    mpContext.resize(streamCnt);
    mpEngine.resize(streamCnt);
    for (uint i = 0; i < streamCnt; ++i)
    {
        mpEngine[i] = std::unique_ptr<nvinfer1::ICudaEngine>(mpRuntime->deserializeCudaEngine(plan->data(), plan->size()));
        if (mpEngine[i].get() == nullptr)
            logFatal("engine is null");

        mpContext[i] = std::unique_ptr<nvinfer1::IExecutionContext>(mpEngine[i]->createExecutionContext());
        if (mpContext[i].get() == nullptr)
            logFatal("context is null");
    }

    FALCOR_CHECK(1 == network->getNbOutputs(), "output tensor number mismatch!");
    auto name = mpEngine[0]->getIOTensorName(mpEngine[0]->getNbIOTensors() - 1);
    Falcor::logInfo("output tensor name {} is {}", 0, name);
    mpOutputNames.push_back(name);
}

Properties Network::getProperties() const
{
    Properties props;
    props[kAccumulatePass] = mAccumulatePass;
    props[kDirectory] = mDirectoryPath;
    props[kNetworkInputLength] = networkInputLength;
    props[kBatchSize] = batchSize;
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
    uint numPatches = mFrameDim.x * mFrameDim.y / batchSize + 1;
    float16_t* base_input_ptr = static_cast<float16_t*>(mpNetworkInputBuffer->getCudaMemory()->getMappedData());
    float16_t* base_output_ptr = static_cast<float16_t*>(mpNetworkOutputBuffer->getCudaMemory()->getMappedData());

    auto inference_start_time = std::chrono::high_resolution_clock::now();
    for (uint i = 0; i < numPatches; i++)
    {
        uint id = i % streamCnt;
        void* inputAddr = base_input_ptr + i * networkInputLength * batchSize;
        bool res = mpContext[id]->setTensorAddress(mpInputNames[0].c_str(), inputAddr);
        if (!res)
            logFatal("Set input tensor {} address failed!", mpInputNames[0]);

        void* outputAddr = base_output_ptr + i * networkInputLength * batchSize * 2;
        res = mpContext[id]->setTensorAddress(mpOutputNames[0].c_str(), outputAddr);
        if (!res)
            logFatal("Set output tensor address failed!");

        mpContext[id]->enqueueV3(mpStream[id]);
    }
    for (uint i = 0; i < streamCnt; i++)
        checkCudaErrorCode(cudaStreamSynchronize(mpStream[i]));

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
    vars["PerFrameCB"]["gFrame"] = mFrame - networkInputLength / 2;
    pRenderContext->clearUAVCounter(mpCompressBuffer, 0);
    vars["buffer_output"] = mpCompressBuffer;

    mpNetworkOutputPass->execute(pRenderContext, uint3(mFrameDim, 1));

    if ( mFrame >= networkInputLength )
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

    auto end_time = std::chrono::high_resolution_clock::now();
    const auto inference_time_milli = 1000.0 * std::chrono::duration_cast<std::chrono::duration<double> >(inference_end_time - inference_start_time).count();
    const auto time_milli = 1000.0 * std::chrono::duration_cast<std::chrono::duration<double> >(end_time - start_time).count();
    Falcor::logInfo("Inference: {:.6f} ms, MyNetworkPass: {:.6f} ms", inference_time_milli, time_milli);
}

void Network::renderUI(Gui::Widgets& widget) {}

Network::~Network()
{
    for (uint i = 0; i < streamCnt; i++)
        checkCudaErrorCode(cudaStreamDestroy(mpStream[i]));
}
