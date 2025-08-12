#include "d3dApp.h"
#include "Test.h"
#include <iostream>
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
)
{
    Test test(hInstance, L"PerformanceTestCase:Sample", 800, 600);
    test.Initialize();
	test.Dispatch();
    auto duration = test.GetDuration();
    void* temp;

    test.mReadBackBuffer.Get()->Map(0, nullptr, &temp);
    wchar_t debugMsg[256];
    for (int i = 0; i < 64; i++)
    {
        swprintf(debugMsg, 256, L"Output[%d] = %f \n", i, *((float*)temp + i));
        OutputDebugString(debugMsg);
    }

    test.mReadBackBuffer.Get()->Unmap(0, nullptr);
    return 0;
}
