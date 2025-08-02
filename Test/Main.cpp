#include "d3dApp.h"
#include "Test.h"
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
)
{
    Test test(hInstance);
    test.Initialize();
    test.Dispatch();
    auto duration = test.GetDuration();
    return 0;
}
