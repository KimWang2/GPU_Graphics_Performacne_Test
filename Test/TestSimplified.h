#pragma once
#include "d3dAppSimplified.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <random>

class TestSimplified : public D3DAppSimplified
{
public:

    struct Vector3D
    {
        float x, y, z;
    };

    TestSimplified(HINSTANCE hInstance) : D3DAppSimplified(hInstance) { } 

    void BuildResourcesAndHeaps() override {
        std::vector<Vector3D> inputVectors(64);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> magnitudeDist(1.0f, 10.0f);
        std::uniform_real_distribution<float> angleDist(0.0f, DirectX::XM_2PI);
		for (auto& vec : inputVectors)
		{
			float magnitude = magnitudeDist(gen);
			float theta = angleDist(gen);    // azimuthal angle
			float phi = angleDist(gen);       // polar angle

			vec.x = magnitude * sinf(phi) * cosf(theta);
			vec.y = magnitude * sinf(phi) * sinf(theta);
			vec.z = magnitude * cosf(phi);
		}

        mDefaultBuffer = D3DUtil::CreateDefaultBuffer(Device(), GraphicsCommandList(), inputVectors.data(), inputVectors.size() * sizeof(Vector3D));
		
		UINT byteSize = 36 * sizeof(float);
		auto temp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto temp1 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
		auto temp3 = CD3DX12_RESOURCE_DESC::Buffer(byteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		auto temp4 = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

		AssertIfFailed(Device()->CreateCommittedResource(
			&temp,
			D3D12_HEAP_FLAG_NONE,
			&temp3,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&mOutputBuffer)));
	
		AssertIfFailed(Device()->CreateCommittedResource(
			&temp1,
			D3D12_HEAP_FLAG_NONE,
			&temp4,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&mReadBackBuffer)));
	
	}

    void BuildShadersAndInputLayout() override {
        mShaders = D3DUtil::CompileShader(L"Shaders\\VectorLengthsSimplified.hlsl", nullptr, "CSMain", "cs_5_0");
    }

    void BuildPSOs() override {
		CD3DX12_ROOT_PARAMETER slotRootParameter[2];

		// Perfomance TIP: Order from most frequent to least frequent.
		slotRootParameter[0].InitAsShaderResourceView(0);
		slotRootParameter[1].InitAsUnorderedAccessView(0);

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

		// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
		ComPtr<ID3DBlob> serializedRootSig = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()); 
		if(errorBlob != nullptr)
		{
			::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		}
		AssertIfFailed(hr);

		AssertIfFailed(Device()->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mRootSignature.GetAddressOf())));

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = mRootSignature.Get();
		computePsoDesc.CS =
		{
			reinterpret_cast<BYTE*>(mShaders->GetBufferPointer()),
			mShaders->GetBufferSize()
		};
		computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		AssertIfFailed(Device()->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSO)));
    }

    void DoAction() override {
	   // Dispatch compute shader
		auto commandList = GraphicsCommandList();
		commandList->SetComputeRootSignature(mRootSignature.Get());
		commandList->SetPipelineState(mPSO.Get());
		commandList->SetComputeRootShaderResourceView(0, mDefaultBuffer->GetGPUVirtualAddress());
		commandList->SetComputeRootUnorderedAccessView(1, mOutputBuffer->GetGPUVirtualAddress());
		commandList->Dispatch(1, 1, 1);

		// Barrier to transition output buffer to copy source
		D3D12_RESOURCE_BARRIER outputBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
			mOutputBuffer.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
			D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &outputBarrier);

		// Copy results to readback buffer
		commandList->CopyResource(mReadBackBuffer.Get(), mOutputBuffer.Get());
    }

    ComPtr<ID3DBlob> mShaders;
    Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> mDefaultBuffer;
	ComPtr<ID3D12Resource> mOutputBuffer = nullptr;
	ComPtr<ID3D12Resource> mReadBackBuffer = nullptr;

	ComPtr<ID3D12RootSignature> mRootSignature;
	ComPtr<ID3D12PipelineState> mPSO;
};

