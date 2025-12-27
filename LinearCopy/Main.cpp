#include "d3dApp.h"
#include "d3dAppSimplified.h"
#include "GpuCopy.h"
#include <iostream>
#include <sstream>
#include <d3dUtil.h>
#include <fstream>
#include <cstdlib>  // for atoi

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
)
{
	int    size      = 1024;
	int    width     = 1024;
	int    height    = 1024;
	int    strideI   = 1024;
	int    strideO   = 1024;
	ShaderType shaderType = ShaderType::Linear;
	double bandwidth = 0;

	// Get command line arguments using Windows API
	// Usage: program.exe <width> <height> <strideI> <strideO> <shaderType>
	// shaderType: 0=Linear, 1=Tiled, etc.
	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	if (argv)
	{
		if (argc >= 2)
		{
			width = _wtoi(argv[1]);
		}
		if (argc >= 3)
		{
			height = _wtoi(argv[2]);
		}
		if (argc >= 4)
		{
			strideI = _wtoi(argv[3]);
		}
		if (argc >= 5)
		{
			strideO = _wtoi(argv[4]);
		}
		if (argc >= 6)
		{
			int shaderTypeInt = _wtoi(argv[5]);
			shaderType = static_cast<ShaderType>(shaderTypeInt);
		}

		// Show parsed arguments in debug output
		wchar_t buffer[512];
		swprintf_s(buffer, L"Params: width=%d, height=%d, strideI=%d, strideO=%d, shaderType=%d\n",
			width, height, strideI, strideO, static_cast<int>(shaderType));
		OutputDebugStringW(buffer);

		LocalFree(argv);  // Free memory allocated by CommandLineToArgvW
	}
	else
	{
		OutputDebugStringA("Failed to parse command line, using defaults\n");
		assert(true);
	}
	
	GpuCopy test(hInstance, static_cast<uint32_t>(width), static_cast<uint32_t>(height), 
		static_cast<uint32_t>(strideO), static_cast<uint32_t>(strideI), shaderType);
	test.Initialize();
	test.Dispatch();
	double duration = test.GetDuration();
	float bytesCopy = test.m_height * test.m_width * sizeof(float) * 1.0f;
	bandwidth = (bytesCopy / duration / 1024 / 1024 / 1024);

	std::ostringstream debugOutput;
	debugOutput << "**************************Summary**************************\n";
	debugOutput << "Height: " << test.m_height << " Width: " << test.m_width << "\n";
	debugOutput << "Total Bytes Copied: " << bytesCopy << " bytes\n";
	debugOutput << "GPU Linear Copy Bandwidth: " << bandwidth << " GB/s\n";
	debugOutput << "GPU Linear Copy Duration:  " << duration << " seconds\n";
	debugOutput << "**************************EndEnd**************************\n";
	D3DUtil::PrintDebugString(debugOutput.str());

	// Append to CSV file with timestamp
	std::ofstream csvfile("bandwidth_results.csv", std::ios::app);
	if (csvfile.is_open()) {
		// Write header if file is empty
		csvfile.seekp(0, std::ios::end);
		if (csvfile.tellp() == 0) {
			csvfile << "Width ,Height, StrideI, StrideO, Bandwidth_GBs\n";
		}
		csvfile << width << "," << height << "," << strideI << "," << strideO << "," << bandwidth << "\n";
		csvfile.close();
	}
    return 0;
}