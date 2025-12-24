#pragma once
#include <d3dApp.h>
#include <cassert>
#include <string>
#include <fstream>
#include <comdef.h> // For _com_error
#include <vector>

//#define AssertIfFailed(x) assert(SUCCEEDED(x))
#define AssertIfFailed(x)                                         \
{                                                                 \
	HRESULT hr__ = (x);                                           \
	if (FAILED(hr__)) {                                           \
		_com_error err(hr__);                                     \
		OutputDebugStringW(err.ErrorMessage());                   \
		throw err.ErrorMessage();                                 \
	}                                                             \
}

namespace D3DUtil
{
    using Microsoft::WRL::ComPtr;  

	static std::vector<ComPtr<ID3D12Resource>> g_uploadBuffers;

    inline ComPtr<ID3DBlob> CompileShader(
        const std::wstring& filename,
        const D3D_SHADER_MACRO* defines,
        const std::string& entrypoint,
        const std::string& target)
    {
        UINT compileFlags = 0;

#if defined(DEBUG) || defined(_DEBUG)  
        compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        HRESULT hr = S_OK;

        ComPtr<ID3DBlob> byteCode = nullptr;
        ComPtr<ID3DBlob> errors;
        hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

        if (errors != nullptr)
            OutputDebugStringA((char*)errors->GetBufferPointer());

        AssertIfFailed(hr);

        return byteCode;
    }

    static Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename)
    {
		std::ifstream fin(filename, std::ios::binary);

		fin.seekg(0, std::ios_base::end);
		std::ifstream::pos_type size = (int)fin.tellg();
		fin.seekg(0, std::ios_base::beg);

		ComPtr<ID3DBlob> blob;
		AssertIfFailed(D3DCreateBlob(size, blob.GetAddressOf()));

		fin.read((char*)blob->GetBufferPointer(), size);
		fin.close();

		return blob;
    }

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const void* initData,
        UINT64 byteSize) 
    {
		ComPtr<ID3D12Resource> uploadBuffer;
		ComPtr<ID3D12Resource> defaultBuffer;
		D3D12_HEAP_PROPERTIES defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_HEAP_PROPERTIES uploadHeap  = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC   bufferSize  = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
		// Create the actual default buffer resource.
		device->CreateCommittedResource(
				&defaultHeap,
				D3D12_HEAP_FLAG_NONE,
				&bufferSize,
				D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS(defaultBuffer.GetAddressOf()));

		// In order to copy CPU memory data into our default buffer, we need to create
		// an intermediate upload heap. 
		device->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&bufferSize,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadBuffer.GetAddressOf()));

		g_uploadBuffers.push_back(uploadBuffer);

		// Describe the data we want to copy into the default buffer.
		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = initData;
		subResourceData.RowPitch = byteSize;
		subResourceData.SlicePitch = subResourceData.RowPitch;

		// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
		// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
		// the intermediate upload heap data will be copied to mBuffer.
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), 
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

		UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

		return defaultBuffer;
    }

	std::wstring StringToWString(const std::string& str) {
		return std::wstring(str.begin(), str.end());
	}
	
	void PrintDebugString(std::string debugOutput) {
		OutputDebugString(StringToWString(debugOutput).c_str());
	}

}

