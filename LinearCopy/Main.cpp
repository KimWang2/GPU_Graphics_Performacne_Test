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
	auto duration = test.GetDuration();
    return 0;
}