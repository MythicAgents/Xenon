#include <windows.h>

WINUSERAPI int WINAPI USER32$MessageBoxA(HWND hWnd,LPCSTR lpText,LPCSTR lpCaption,UINT uType);

void go() {
	USER32$MessageBoxA(NULL, "Hello World (COFF)", "Test!", MB_OK);
}
