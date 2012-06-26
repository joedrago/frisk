#include <windows.h>

#include "FriskWindow.h"

INT_PTR CALLBACK FriskProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
    LoadLibrary(TEXT("RICHED20.DLL"));

    FriskWindow window(hInstance);
    window.show();
	return 0;
}
