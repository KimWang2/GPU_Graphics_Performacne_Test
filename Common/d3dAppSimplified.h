#pragma once

#include <windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl.h>
#include <d3dcompiler.h>
#include "d3dx12.h"
#include "d3dUtil.h"
#include <string>
#include <cassert>
#include <DirectXMath.h>
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

// Link necessary d3d12 libraries.
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

class D3DAppSimplified
{
public:
    
    D3DAppSimplified(HINSTANCE hInstance) : mhAppInst(hInstance) { }


	virtual ~D3DAppSimplified() {
        if (mhAppInst) {
            if (!UnregisterClass(L"MainWnd", mhAppInst)) {
                DWORD error = GetLastError();
                // Log error if needed
            }
        }
    }

    void Initialize() {
        InitMainWindow();
		InitGraphics();
    }

    virtual void BuildResourcesAndHeaps() = 0;
    virtual void BuildShadersAndInputLayout() = 0;
    virtual void BuildPSOs() = 0;
    virtual void DoAction()  = 0;

    void Dispatch(){

        mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);
        // Transition the resource from its initial state to be used as a depth buffer.
        mCommandList->EndQuery(mTimestampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);

        DoAction();

        mCommandList->EndQuery(mTimestampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

        // Resolve the timestamp data to the readback buffer
        mCommandList->ResolveQueryData(mTimestampQueryHeap.Get(),
                                       D3D12_QUERY_TYPE_TIMESTAMP,
                                       0,              // Start index
                                       2,              // Query count (start + end)
                                       mTimestampQueryReadbackBuffer.Get(),
                                       0);             // Offset into buffer   

        // Wait until resize is complete.
        SubmitAndFlushCommandQueue();
    }

    UINT GetCbvSrvUavDescriptorSize() { return mCbvSrvUavDescriptorSize; }

    double GetDuration()
    {
        // Map the readback buffer
        UINT64* pTimestampData;
        D3D12_RANGE readRange = {0, mTimestampQueryBufferSize};
        mTimestampQueryReadbackBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pTimestampData));
        UINT64 startTimestamp = pTimestampData[0];
        UINT64 endTimestamp   = pTimestampData[1];
        mTimestampQueryReadbackBuffer->Unmap(0, nullptr);

        // Get GPU timestamp frequency (ticks per second)
        UINT64 timestampFrequency;
        mCommandQueue->GetTimestampFrequency(&timestampFrequency);

        // Calculate duration in seconds
        double gpuDuration = static_cast<double>(endTimestamp - startTimestamp) / timestampFrequency;

        return gpuDuration;
    }
    
    ID3D12Device* Device() const
    {
        return md3dDevice.Get();
    }

    ID3D12GraphicsCommandList* GraphicsCommandList() const
    {
        return mCommandList.Get();
    }

private:

	void CreateSwapChainDepthBufferAndView()
    {
       // Release the previous swapchain we will be recreating.
        mSwapChain.Reset();

        DXGI_SWAP_CHAIN_DESC sd;
        sd.BufferDesc.Width = mClientWidth;
        sd.BufferDesc.Height = mClientHeight;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = SwapChainBufferCount;
        sd.OutputWindow = mhMainWnd;
        sd.Windowed = true;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        // Note: Swap chain uses queue to perform flush.
        AssertIfFailed(mdxgiFactory->CreateSwapChain(
                        mCommandQueue.Get(),
                        &sd, 
                        mSwapChain.GetAddressOf())); 

        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
        rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
        rtvHeapDesc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtvHeapDesc.NodeMask = 0;
        AssertIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT i = 0; i < SwapChainBufferCount; i++)
        {
            AssertIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
            md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
            rtvHeapHandle.Offset(1, mRtvDescriptorSize);
        }
    }

    bool InitMainWindow()
    {
    	WNDCLASS wc;
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = DefWindowProc; 
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = mhAppInst;
        wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
        wc.hCursor       = LoadCursor(0, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        wc.lpszMenuName  = 0;
        wc.lpszClassName = L"MainWnd";

        if(!RegisterClass(&wc) )
        {
            MessageBox(0, L"RegisterClass Failed.", 0, 0);
            return false;
        }

        // Compute window rectangle dimensions based on requested client area dimensions.
        RECT R = { 0, 0, mClientWidth, mClientHeight };
        AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
        int width  = R.right - R.left;
        int height = R.bottom - R.top;

        mhMainWnd = CreateWindow(L"MainWnd", L"PerformanceTest", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, mClientWidth, mClientHeight, 0, 0, mhAppInst, 0);
        if( !mhMainWnd )
        {
            MessageBox(0, L"CreateWindow Failed.", 0, 0);
            return false;
        }

        ShowWindow(mhMainWnd, SW_SHOW);
        UpdateWindow(mhMainWnd);

        return true;
    }

    bool InitGraphics()
    {
#if defined(DEBUG) || defined(_DEBUG) 
        {
            ComPtr<ID3D12Debug> debugController;
            AssertIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
            debugController->EnableDebugLayer();
        } 
#endif
        AssertIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));
       
        // Try to create hardware device.
        AssertIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&md3dDevice)));
        //assert(SUCCEEDED(hardwareResult));
    
        AssertIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
    
		mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); 

        CreateCommandObjects();

		AssertIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

        BuildResourcesAndHeaps();
        BuildShadersAndInputLayout();
        BuildPSOs();
        CreateQueryHeapAndResorce();
        CreateSwapChainDepthBufferAndView();
        SubmitAndFlushCommandQueue();

        return true;
    }

    void CreateQueryHeapAndResorce()
    {
        // Create timestamp query heap
        D3D12_QUERY_HEAP_DESC timestampHeapDesc = {};
        timestampHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        timestampHeapDesc.Count = 2;  // Need at least 2 for start/end timestamps
        timestampHeapDesc.NodeMask = 0;

        AssertIfFailed(md3dDevice->CreateQueryHeap(&timestampHeapDesc, IID_PPV_ARGS(&mTimestampQueryHeap)));

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_READBACK;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = mTimestampQueryBufferSize;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        AssertIfFailed(md3dDevice->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&mTimestampQueryReadbackBuffer)));
    }
    
    void SubmitCommand()
    {
        AssertIfFailed(mCommandList->Close());
        ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
        mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists); 
    }

    void SubmitAndFlushCommandQueue()
    {
        SubmitCommand();
    
        mSwapChain->Present(0, 0);

        // Advance the fence value to mark commands up to this fence point.
        mCurrentFence++;

        // Add an instruction to the command queue to set a new fence point.  Because we 
        // are on the GPU timeline, the new fence point won't be set until the GPU finishes
        // processing all the commands prior to this Signal().
        AssertIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

        // Wait until the GPU has completed commands up to this fence point.
        if(mFence->GetCompletedValue() < mCurrentFence)
        {
            HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);

            // Fire event when GPU hits current fence.  
            AssertIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

            // Wait until the GPU hits current fence event is fired.
            WaitForSingleObject(eventHandle, INFINITE);
            CloseHandle(eventHandle);
        } 
    }

	void CreateCommandObjects()
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        AssertIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

        AssertIfFailed(md3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));

        AssertIfFailed(md3dDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    mDirectCmdListAlloc.Get(), // Associated command allocator
                                                    nullptr,                   // Initial PipelineStateObject
                                                    IID_PPV_ARGS(mCommandList.GetAddressOf())));

        // Start off in a closed state.  This is because the first time we refer 
        // to the command list we will Reset it, and it needs to be closed before
        // calling Reset.
        mCommandList->Close(); 
    }

    HINSTANCE mhAppInst     = nullptr;

	UINT mCbvSrvUavDescriptorSize = 0;
    Microsoft::WRL::ComPtr<IDXGIFactory4>  mdxgiFactory;
    Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Device>   md3dDevice;
    Microsoft::WRL::ComPtr<ID3D12Fence>    mFence;
    UINT64 mCurrentFence = 0;

    D3D_DRIVER_TYPE md3dDriverType  = D3D_DRIVER_TYPE_HARDWARE;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue>        mCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    mDirectCmdListAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

    Microsoft::WRL::ComPtr<ID3D12QueryHeap> mTimestampQueryHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource>  mTimestampQueryReadbackBuffer;
    UINT64 mTimestampQueryBufferSize = 2 * sizeof(UINT64); // For two timestamps

    
	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = -1;
    Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	HWND      mhMainWnd     = nullptr;
    bool      mShowWindow   = false;
    int       mClientWidth  = 800;
    int       mClientHeight = 800;
	UINT mRtvDescriptorSize = 0;

};
