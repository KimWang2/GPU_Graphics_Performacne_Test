#include "d3dApp.h"
#include "d3dAppSimplified.h"
#include "GpuCopy.h"
#include <iostream>
#include <sstream>
#include <d3dUtil.h>

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
)
{
	std::vector<int> sizes;
	std::vector<double> bandwidths;

    for (int i = 8; i <= 1024; i = i + 8)
    {
		uint32_t width = i;
		uint32_t height = i;
		GpuCopy test(hInstance, width, height);
		test.Initialize();
		test.Dispatch();
		double duration  = test.GetDuration();
		float bytesCopy  = test.m_height * test.m_width * sizeof(float) * 1.0f;
		double bandwidth = bytesCopy / duration / 1024 / 1024 / 1024;

		sizes.push_back(i);
		bandwidths.push_back(bandwidth);

		std::ostringstream debugOutput;
		debugOutput << "**************************Summary**************************\n";
		debugOutput << "Height: " << test.m_height << " Width: " << test.m_width << "\n";
		debugOutput << "Total Bytes Copied: " << bytesCopy << " bytes\n";
		debugOutput << "GPU Linear Copy Bandwidth: " << bandwidth << " GB/s\n";
		debugOutput << "GPU Linear Copy Duration:  " << duration << " seconds\n";
		debugOutput << "**************************EndEnd**************************\n";

		D3DUtil::PrintDebugString(debugOutput.str());
    }
	
	// Output sizes as Python list
	std::ostringstream pythonOutput;
	pythonOutput << "\n# Python visualization data\n";
	pythonOutput << "sizes = [";
	for (size_t i = 0; i < sizes.size(); ++i)
	{
		pythonOutput << sizes[i];
		if (i < sizes.size() - 1) pythonOutput << ", ";
	}
	pythonOutput << "]\n";

	// Output bandwidths as Python list
	pythonOutput << "bandwidths = [";
	for (size_t i = 0; i < bandwidths.size(); ++i)
	{
		pythonOutput << bandwidths[i];
		if (i < bandwidths.size() - 1) pythonOutput << ", ";
	}
	pythonOutput << "]\n";

	// Add sample Python plot code
	pythonOutput << "\n# Sample matplotlib code:\n";
	pythonOutput << "# import matplotlib.pyplot as plt\n";
	pythonOutput << "# plt.plot(sizes, bandwidths)\n";
	pythonOutput << "# plt.xlabel('Size (pixels)')\n";
	pythonOutput << "# plt.ylabel('Bandwidth (GB/s)')\n";
	pythonOutput << "# plt.title('GPU Linear Copy Bandwidth')\n";
	pythonOutput << "# plt.show()\n";

	D3DUtil::PrintDebugString(pythonOutput.str());

    return 0;
}