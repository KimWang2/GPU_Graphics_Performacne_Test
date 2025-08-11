#pragma once
#include "d3dApp.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <random>

class Test : public D3DApp
{
public:

    struct Vector3D
    {
        float x, y, z;
    };

    Test(HINSTANCE hInstance) : D3DApp(hInstance) { } 

    Test(HINSTANCE hInstance, std::wstring caption, int windowWidth, int windowHeight) : D3DApp(hInstance, caption, windowWidth, windowHeight){}

    void BuildResourcesAndHeaps() override {
        std::vector<Vector3D> inputVectors(64);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> magnitudeDist(1.0f, 100.0f);
        std::uniform_real_distribution<float> angleDist(0.0f, DirectX::XM_2PI);
		for (auto& vec : inputVectors)
		{
			float magnitude = magnitudeDist(gen);
			float theta = angleDist(gen);     // azimuthal angle
			float phi = angleDist(gen);       // polar angle

			vec.x = magnitude * sinf(phi) * cosf(theta);
			vec.y = magnitude * sinf(phi) * sinf(theta);
			vec.z = magnitude * cosf(phi);
		}

        mInputBuffer = D3DUtil::CreateDefaultBuffer(Device(), GraphicsCommandList(), inputVectors.data(), inputVectors.size() * sizeof(Vector3D), mUploadBuffer);
		
		UINT byteSize     = 64 * sizeof(float);
		auto defaultHeap  = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto readbackHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
		auto uavDesc      = CD3DX12_RESOURCE_DESC::Buffer(byteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		auto defaultDesc  = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

		AssertIfFailed(Device()->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&uavDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&mOutputBuffer)));
	
		AssertIfFailed(Device()->CreateCommittedResource(
			&readbackHeap,
			D3D12_HEAP_FLAG_NONE,
			&defaultDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&mReadBackBuffer)));
	}

    void BuildShadersAndInputLayout() override {
        mShaders = D3DUtil::CompileShader(L"Shaders\\VectorLengths.hlsl", nullptr, "CSMain", "cs_5_0");
    }


    void DoAction() override {
	   // Dispatch compute shader
		auto commandList = GraphicsCommandList();

		commandList->Dispatch(1, 1, 1);

		// Barrier to transition output buffer to copy source
		D3D12_RESOURCE_BARRIER outputBarrier = CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
																					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
																					D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &outputBarrier);

		// Copy results to readback buffer
	    commandList->CopyResource(mReadBackBuffer.Get(), mOutputBuffer.Get());
    }

	ID3D10Blob* GetComputerShader() override { return mShaders.Get(); }

	void SetCBV(CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHeap) override { }

	void SetSRV(CD3DX12_CPU_DESCRIPTOR_HANDLE srvHeap) override {
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		srvDesc.Buffer.FirstElement = 0,
		srvDesc.Buffer.NumElements = 64,
		srvDesc.Buffer.StructureByteStride = 0, // Not structured, just raw float3
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE,
		Device()->CreateShaderResourceView(mInputBuffer.Get(), &srvDesc, srvHeap);	
	}

	void SetUAV(CD3DX12_CPU_DESCRIPTOR_HANDLE uavHeap) override { 
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		uavDesc.Format              = DXGI_FORMAT_R32_FLOAT, // float
		uavDesc.ViewDimension       = D3D12_UAV_DIMENSION_BUFFER,
		uavDesc.Buffer.FirstElement = 0,
		uavDesc.Buffer.NumElements  = 64,
		uavDesc.Buffer.StructureByteStride = 0, // Not structured, just raw float
		uavDesc.Buffer.CounterOffsetInBytes = 0, // No counter needed
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE,

		// Offset to the second descriptor in the heap
		Device()->CreateUnorderedAccessView(mOutputBuffer.Get(), nullptr, &uavDesc, uavHeap);
	}


    ComPtr<ID3DBlob> mShaders;
    ComPtr<ID3D12Resource> mUploadBuffer;
    ComPtr<ID3D12Resource> mInputBuffer;
	ComPtr<ID3D12Resource> mOutputBuffer;
	ComPtr<ID3D12Resource> mReadBackBuffer;
};

