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
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

// Link necessary d3d12 libraries.
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

class D3DApp
{
public:
    
    D3DApp(HINSTANCE hInstance) : mhAppInst(hInstance) 
    {
        assert(InitGraphics(false));
    }

    D3DApp(HINSTANCE hInstance, std::wstring caption, int windowWidth, int windowHeight) : mhAppInst(hInstance), mWindowCaption(caption), mClientHeight(windowHeight), mClientWidth(windowWidth)
    {
        assert(InitMainWindow());
        assert(InitGraphics(true));
    }
    
    virtual void BuildResourcesAndHeaps() = 0;
    virtual void BuildDescriptorHeaps() = 0;
    virtual void BuildShadersAndInputLayout() = 0;
    virtual void BuildPSOs() = 0;
    virtual void DoAction()  = 0;

    void Draw() {}

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

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = mTimestampQueryReadbackBuffer.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        mCommandList->ResourceBarrier(1, &barrier);


        AssertIfFailed(mCommandList->Close());
        ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
        mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

        // Wait until resize is complete.
        FlushCommandQueue();
    }


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

    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const
    {
        return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
    }
    
    ID3D12GraphicsCommandList* GraphicsCommandList() const
    {
        return mCommandList.Get();
    }

private:

    bool InitGraphics(bool hasWindow)
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
        mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        
        CreateCommandObjects();

        if (hasWindow)
        {
            CreateSwapChainDepthBufferAndView();
        }

        BuildResourcesAndHeaps();
        BuildDescriptorHeaps();
        BuildShadersAndInputLayout();
        BuildPSOs();

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

    void FlushCommandQueue()
    {
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

        if( !RegisterClass(&wc) )
        {
            MessageBox(0, L"RegisterClass Failed.", 0, 0);
            return false;
        }

        // Compute window rectangle dimensions based on requested client area dimensions.
        RECT R = { 0, 0, mClientWidth, mClientHeight };
        AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
        int width  = R.right - R.left;
        int height = R.bottom - R.top;

        mhMainWnd = CreateWindow(L"MainWnd", mWindowCaption.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0); 
        if( !mhMainWnd )
        {
            MessageBox(0, L"CreateWindow Failed.", 0, 0);
            return false;
        }

        ShowWindow(mhMainWnd, SW_SHOW);
        UpdateWindow(mhMainWnd);

        return true;
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

    void CreateSwapChainDepthBufferAndView()
    {
       // Release the previous swapchain we will be recreating.
        mSwapChain.Reset();

        DXGI_SWAP_CHAIN_DESC sd;
        sd.BufferDesc.Width = mClientWidth;
        sd.BufferDesc.Height = mClientHeight;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferDesc.Format = mBackBufferFormat;
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


        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        dsvHeapDesc.NodeMask = 0;
        AssertIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));


        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT i = 0; i < SwapChainBufferCount; i++)
        {
            AssertIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
            md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
            rtvHeapHandle.Offset(1, mRtvDescriptorSize);
        }

        
        // Create the depth/stencil buffer and view.
        D3D12_RESOURCE_DESC depthStencilDesc;
        depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthStencilDesc.Alignment = 0;
        depthStencilDesc.Width = mClientWidth;
        depthStencilDesc.Height = mClientHeight;
        depthStencilDesc.DepthOrArraySize = 1;
        depthStencilDesc.MipLevels = 1;

        depthStencilDesc.Format = mDepthStencilFormat;

        depthStencilDesc.SampleDesc.Count   = 1;
        depthStencilDesc.SampleDesc.Quality = 0;
        depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE optClear;
        optClear.Format = mDepthStencilFormat;
        optClear.DepthStencil.Depth = 1.0f;
        optClear.DepthStencil.Stencil = 0;
        
        AssertIfFailed(md3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                           D3D12_HEAP_FLAG_NONE,
                                                           &depthStencilDesc,
                                                           D3D12_RESOURCE_STATE_COMMON,
                                                           &optClear,
                                                           IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

        // Create descriptor to mip level 0 of entire resource using the format of the resource.
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Format = mDepthStencilFormat;
        dsvDesc.Texture2D.MipSlice = 0;
        md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());


        mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);
        // Transition the resource from its initial state to be used as a depth buffer.
        
        mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
        
        // Execute the resize commands.
        AssertIfFailed(mCommandList->Close());
        ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
        mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

        // Wait until resize is complete.
        FlushCommandQueue();

        // Update the viewport transform to cover the client area.
        mScreenViewport.TopLeftX = 0;
        mScreenViewport.TopLeftY = 0;
        mScreenViewport.Width    = static_cast<float>(mClientWidth);
        mScreenViewport.Height   = static_cast<float>(mClientHeight);
        mScreenViewport.MinDepth = 0.0f;
        mScreenViewport.MaxDepth = 1.0f;

        mScissorRect = { 0, 0, mClientWidth, mClientHeight };

    }


    HINSTANCE mhAppInst     = nullptr;
    HWND      mhMainWnd     = nullptr;
    bool      mShowWindow   = false;
    int       mClientWidth  = 0;
    int       mClientHeight = 0;
    std::wstring mWindowCaption;

    UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;
    Microsoft::WRL::ComPtr<IDXGIFactory4>  mdxgiFactory;
    Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Device>   md3dDevice;
    Microsoft::WRL::ComPtr<ID3D12Fence>    mFence;
    UINT64 mCurrentFence = 0;

    D3D_DRIVER_TYPE md3dDriverType  = D3D_DRIVER_TYPE_HARDWARE;
    DXGI_FORMAT mBackBufferFormat   = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
    Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

    Microsoft::WRL::ComPtr<ID3D12QueryHeap> mTimestampQueryHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource>  mTimestampQueryReadbackBuffer;
    UINT64 mTimestampQueryBufferSize = 2 * sizeof(UINT64); // For two timestamps

    D3D12_VIEWPORT mScreenViewport; 
    D3D12_RECT mScissorRect;
};
