#pragma once

#include <windows.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <map>
#include <functional>

// GPUPerfAPI headers
#include "gpu_performance_api/gpu_perf_api.h"
#include "gpu_performance_api/gpu_perf_api_types.h"
#include "gpu_performance_api/gpu_perf_api_functions.h"

class GPAWrapper
{
public:
    GPAWrapper() = default;
    ~GPAWrapper() { Shutdown(); }

    bool Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue)
    {
        // Load GPA DLL
        mGPAModule = LoadLibraryA("C:\\Users\\kimwang2\\Desktop\\GPU_Graphics_Performacne_Test\\GPUPerfAPI\\bin\\GPUPerfAPIDX12-x64.dll");
        if (!mGPAModule)
        {
            OutputDebugStringA("Failed to load GPUPerfAPI DLL\n");
            return false;
        }

        // Get function table
        auto GpaGetFuncTable = reinterpret_cast<GpaGetFuncTablePtrType>(
            GetProcAddress(mGPAModule, "GpaGetFuncTable"));
        if (!GpaGetFuncTable)
        {
            OutputDebugStringA("Failed to get GpaGetFuncTable\n");
            return false;
        }

        GpaStatus status = GpaGetFuncTable(reinterpret_cast<void**>(&mGpaFuncTable));
        if (status != kGpaStatusOk)
        {
            OutputDebugStringA("Failed to get GPA function table\n");
            return false;
        }

        // Initialize GPA
        status = mGpaFuncTable->GpaInitialize(kGpaInitializeDefaultBit);
        if (status != kGpaStatusOk)
        {
            OutputDebugStringA("GpaInitialize failed\n");
            return false;
        }

        // Open context for D3D12
        GpaOpenContextFlags flags = kGpaOpenContextDefaultBit;
        status = mGpaFuncTable->GpaOpenContext(commandQueue, flags, &mGpaContextId);
        if (status != kGpaStatusOk)
        {
            OutputDebugStringA("GpaOpenContext failed\n");
            return false;
        }

        mDevice = device;
        mCommandQueue = commandQueue;
        mInitialized = true;

        // Cache available counters
        CacheCounterInfo();

        return true;
    }

    void Shutdown()
    {
        if (mGpaFuncTable && mGpaContextId)
        {
            mGpaFuncTable->GpaCloseContext(mGpaContextId);
            mGpaFuncTable->GpaDestroy();
        }
        if (mGPAModule)
        {
            FreeLibrary(mGPAModule);
            mGPAModule = nullptr;
        }
        mInitialized = false;
    }

    bool CreateSession()
    {
        if (!mInitialized) return false;

        GpaStatus status = mGpaFuncTable->GpaCreateSession(
            mGpaContextId, kGpaSessionSampleTypeDiscreteCounter, &mGpaSessionId);
        return status == kGpaStatusOk;
    }

    bool EnableCounter(const char* counterName)
    {
        if (!mInitialized || !mGpaSessionId) return false;

        GpaUInt32 counterIndex;
        GpaStatus status = mGpaFuncTable->GpaGetCounterIndex(mGpaSessionId, counterName, &counterIndex);
        if (status != kGpaStatusOk) return false;

        status = mGpaFuncTable->GpaEnableCounter(mGpaSessionId, counterIndex);
        return status == kGpaStatusOk;
    }

    bool EnableAllCounters()
    {
        if (!mInitialized || !mGpaSessionId) return false;

        GpaStatus status = mGpaFuncTable->GpaEnableAllCounters(mGpaSessionId);
        return status == kGpaStatusOk;
    }

    bool BeginSession()
    {
        if (!mInitialized || !mGpaSessionId) return false;

        GpaStatus status = mGpaFuncTable->GpaBeginSession(mGpaSessionId);
        return status == kGpaStatusOk;
    }

    bool EndSession()
    {
        if (!mInitialized || !mGpaSessionId) return false;

        GpaStatus status = mGpaFuncTable->GpaEndSession(mGpaSessionId);
        return status == kGpaStatusOk;
    }

    bool BeginCommandList(ID3D12GraphicsCommandList* cmdList, GpaCommandListId* outCmdListId)
    {
        if (!mInitialized || !mGpaSessionId) return false;

        GpaStatus status = mGpaFuncTable->GpaBeginCommandList(
            mGpaSessionId, 0, cmdList, kGpaCommandListPrimary, outCmdListId);
        return status == kGpaStatusOk;
    }

    bool EndCommandList(GpaCommandListId cmdListId)
    {
        if (!mInitialized) return false;

        GpaStatus status = mGpaFuncTable->GpaEndCommandList(cmdListId);
        return status == kGpaStatusOk;
    }

    bool BeginSample(GpaCommandListId cmdListId, GpaUInt32 sampleId)
    {
        if (!mInitialized) return false;

        GpaStatus status = mGpaFuncTable->GpaBeginSample(sampleId, cmdListId);
        return status == kGpaStatusOk;
    }

    bool EndSample(GpaCommandListId cmdListId)
    {
        if (!mInitialized) return false;

        GpaStatus status = mGpaFuncTable->GpaEndSample(cmdListId);
        return status == kGpaStatusOk;
    }

    bool IsSessionComplete()
    {
        if (!mInitialized || !mGpaSessionId) return false;

        GpaStatus status = mGpaFuncTable->GpaIsSessionComplete(mGpaSessionId);
        return status == kGpaStatusOk;
    }

    bool GetSampleResult(GpaUInt32 sampleId, const char* counterName, double& result)
    {
        if (!mInitialized || !mGpaSessionId) return false;

        GpaUInt32 counterIndex;
        GpaStatus status = mGpaFuncTable->GpaGetCounterIndex(mGpaSessionId, counterName, &counterIndex);
        if (status != kGpaStatusOk) return false;

        size_t sampleResultSize;
        status = mGpaFuncTable->GpaGetSampleResultSize(mGpaSessionId, sampleId, &sampleResultSize);
        if (status != kGpaStatusOk) return false;

        std::vector<unsigned char> resultBuffer(sampleResultSize);
        status = mGpaFuncTable->GpaGetSampleResult(mGpaSessionId, sampleId, sampleResultSize, resultBuffer.data());
        if (status != kGpaStatusOk) return false;

        // Result is stored as double for each enabled counter
        result = reinterpret_cast<double*>(resultBuffer.data())[counterIndex];
        return true;
    }

    void PrintAvailableCounters()
    {
        if (!mInitialized) return;

        GpaUInt32 numCounters;
        mGpaFuncTable->GpaGetNumCounters(mGpaSessionId, &numCounters);

    }

    GpaUInt32 GetNumRequiredPasses()
    {
        if (!mInitialized || !mGpaSessionId) return 0;

        GpaUInt32 numPasses;
        mGpaFuncTable->GpaGetPassCount(mGpaSessionId, &numPasses);
        return numPasses;
    }

private:
    void CacheCounterInfo()
    {
        GpaUInt32 numCounters;
        mGpaFuncTable->GpaGetNumCounters(mGpaSessionId, &numCounters);

        for (GpaUInt32 i = 0; i < numCounters; ++i)
        {
            const char* name;
            mGpaFuncTable->GpaGetCounterName(mGpaSessionId, i, &name);
            mCounterIndexMap[name] = i;
        }
    }

    HMODULE mGPAModule = nullptr;
    GpaFunctionTable* mGpaFuncTable = nullptr;
    GpaContextId mGpaContextId = nullptr;
    GpaSessionId mGpaSessionId = nullptr;
    ID3D12Device* mDevice = nullptr;
    ID3D12CommandQueue* mCommandQueue = nullptr;
    bool mInitialized = false;
    std::map<std::string, GpaUInt32> mCounterIndexMap;
};