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

    void BuildResourcesAndHeaps() {
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
    }

    void BuildDescriptorHeaps() {

    }

    void BuildShadersAndInputLayout() {
        mShaders["VectorLengths"] = D3DUtil::CompileShader(L"Shaders\\VectorLengths.hlsl", nullptr, "CSMain", "cs_5_0");
    }

    void BuildPSOs() {

    }

    void DoAction() {

    }

    std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
};

