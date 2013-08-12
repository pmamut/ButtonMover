#include <windows.h>

static HINSTANCE hMyInst = NULL;

DWORD WINAPI HelperWatcher(LPVOID)
{
	char File[_MAX_PATH + 1];
	if (GetModuleFileName(hMyInst, File, sizeof(File)))
	{
		HANDLE hCurrProc = OpenProcess(PROCESS_ALL_ACCESS, TRUE, GetCurrentProcessId());
		if (hCurrProc)
		{
			char CmdLine[_MAX_PATH + 1];
			wsprintf(CmdLine, "\"%s\" %ld", File, hCurrProc);
			for (;;)
			{
				PROCESS_INFORMATION pi = {NULL, NULL, 0, 0};
				STARTUPINFO si = {sizeof(STARTUPINFO), NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL};
				if (CreateProcess(NULL, CmdLine, NULL, NULL, TRUE, DETACHED_PROCESS, NULL, NULL, &si, &pi))
				{
					WaitForSingleObject(pi.hProcess, INFINITE);
				}
				else
					break;
			}

			CloseHandle(hCurrProc);
		}
	}

	return 0;
}

static long my_atol(LPCSTR szStr)
{
	long lRetVal = 0;
	LPCTSTR pCh = szStr;

	while ((*pCh) && IsCharAlphaNumeric(*pCh) && !IsCharAlpha(*pCh))
	{
		lRetVal = lRetVal * 10 + (*pCh - '0');
		pCh = CharNext(pCh);
	}

	return lRetVal;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow)	
{
	hMyInst = hInst;
	char File[_MAX_PATH + 1];
	if (GetModuleFileName(hMyInst, File, sizeof(File)))
	{
		if (*lpCmdLine)
		{
			HANDLE hMainProc = (HANDLE) my_atol(lpCmdLine);
			if (hMainProc)
			{
				WaitForSingleObject(hMainProc, INFINITE);
				CloseHandle(hMainProc);
//				WinExec(File, SW_SHOWNORMAL);
				PROCESS_INFORMATION pi = {NULL, NULL, 0, 0};
				STARTUPINFO si = {sizeof(STARTUPINFO), NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL};
				CreateProcess(NULL, File, NULL, NULL, FALSE, DETACHED_PROCESS, NULL, NULL, &si, &pi);
			}
		}
		else
		{
			HINSTANCE hCore = LoadLibrary(File);
			if (hCore)
			{
				FARPROC pRegister = GetProcAddress(hCore, "RegisterMover");
				FARPROC pUnregister = GetProcAddress(hCore, "UnregisterMover");
				HHOOK hHook = (HHOOK) (*pRegister)();

				DWORD dwWatcherId;
				HANDLE hWatcher = CreateThread(NULL, 0, HelperWatcher, NULL, 0, &dwWatcherId);
				if (hWatcher)
					WaitForSingleObject(hWatcher, INFINITE);

				(*pUnregister)();

				FreeLibrary(hCore);
			}
		}
	}

	return 0;
}
