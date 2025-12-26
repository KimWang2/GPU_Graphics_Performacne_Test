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
	double bandwidth = 0;
	// Get command line arguments using Windows API
	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	if (argv && argc >= 2)
	{
		// Convert wide string to integer
		size = _wtoi(argv[1]);

		// Optional: Show in debug output
		wchar_t buffer[256];
		swprintf_s(buffer, L"Using size from command line: %d\n", size);
		OutputDebugStringW(buffer);
	}
	else
	{
		OutputDebugStringA("No size provided, using default: 2048\n");
	}

	uint32_t width  = size;
	uint32_t height = size;
	GpuCopy test(hInstance, width, height);
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
			csvfile << "Size, Bandwidth_GBs\n";
		}
		csvfile << size << "," << bandwidth << "\n";
		csvfile.close();
	}
    return 0;
}