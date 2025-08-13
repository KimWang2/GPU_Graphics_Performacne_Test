#include "d3dApp.h"
#include "d3dAppSimplified.h"
#include "GpuCopy.h"
#include <iostream>
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
)
{
	GpuCopy test(hInstance);
	test.Initialize();
	test.Dispatch();
	double duration = test.GetDuration();
    float bytesCopy = test.m_height * test.m_width * sizeof(float) * 1.0f;
    double bandwidth = bytesCopy / duration / 1024 / 1024 / 1024;
    return 0;
}