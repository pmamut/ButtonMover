#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <limits.h>

#define DllImport	__declspec( dllimport )
#define DllExport	__declspec( dllexport )

static HHOOK hHook;
static HINSTANCE hDLL;

typedef enum { LEFT = 0, RIGHT, UP, DOWN, NUM_DIR } direction;

typedef struct
{
	BOOL valid;
	int distance;
	int margin;
} move;
typedef move* pmove;

#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>

void dump(LPCSTR szFormat, ...)
{
	FILE* f = fopen("c:\\dump.txt", "a+");
	va_list arg_ptr;
	va_start(arg_ptr, szFormat);
	vfprintf(f, szFormat, arg_ptr);
	fprintf(f, "\n");
	fclose(f);
}
#else
void dump(LPCSTR szFormat, ...){};
#endif

static void CalculateMove(direction dir, pmove mv, LPPOINT pt, LPRECT target, LPRECT parent)
{
	int distance = 0;
	int margin = 0;

	switch (dir)
	{
	case LEFT:
		distance = pt->x - target->left + 1;
		margin = parent->right - target->right;
		break;

	case RIGHT:
		distance = target->right - pt->x + 1;
		margin = target->left - parent->left;
		break;

	case UP:
		distance = pt->y - target->top + 1;
		margin = parent->bottom - target->bottom;
		break;

	case DOWN:
		distance = target->bottom - pt->y + 1;
		margin = target->top - parent->top;
		break;
	}

	dump("%d: %d, %d", dir, distance, margin);

	margin /= 2; // move half-way
	mv->valid = distance <= margin; // half-way the opposite way is far enough to move the button out of the mouse
	mv->distance = distance;
	mv->margin = margin;
}

static void PerformMove(direction dir, int delta, HWND hWnd, LPPOINT pt, LPRECT target)
{
	RECT dest = *target;
	int i, increment;

	if (dir & 0x1) // right, down
		increment = -1;
	else
		increment = 1;

	for (i = 0; i < delta; i++)
	{
		if (dir & 0x2) // up, down
		{
			dest.top += increment;
			dest.bottom += increment;
		}
		else // left, right
		{
			dest.left += increment;
			dest.right += increment;
		}

		SetWindowPos(hWnd, HWND_TOP, dest.left, dest.top, dest.right - dest.left, dest.bottom - dest.top, SWP_SHOWWINDOW);
	}
	RedrawWindow(hWnd, NULL, NULL, RDW_UPDATENOW | RDW_INVALIDATE | RDW_ERASE);
}

static HWND GetRealParent(HWND hWnd)
{
	HWND hRet = GetParent(hWnd);

	while (hRet && !(GetWindowLong(hRet, GWL_STYLE) & (WS_POPUP | WS_OVERLAPPED)))
		hRet = GetParent(hRet);

	return hRet;
}

static void GetAdjustedWindowRect(HWND hWnd, LPRECT dest)
{
	HWND hParent = GetParent(hWnd);
	POINT pt;

	GetWindowRect(hWnd, dest);

	pt.x = dest->left;
	pt.y = dest->top;
	ScreenToClient(hParent, &pt);
	dest->left = pt.x;
	dest->top = pt.y;

	pt.x = dest->right;
	pt.y = dest->bottom;
	ScreenToClient(hParent, &pt);
	dest->right = pt.x;
	dest->bottom = pt.y;
}

static void DoMaxMarginMove(HWND hWnd, LPPOINT pt, LPRECT target, LPRECT parent)
{
	move moves[NUM_DIR];
	int max_value = INT_MIN;
	direction max_index = NUM_DIR;
	direction i;

	dump("MaxMargin");
	for (i = LEFT; i < NUM_DIR; i++)
		CalculateMove(i, moves + i, pt, target, parent);

	for (i = LEFT; i < NUM_DIR; i++)
	{
		if (moves[i].valid && (moves[i].margin > max_value))
		{
			max_value = moves[i].margin;
			max_index = i;
		}
	}

	if ((max_index < NUM_DIR) && (max_value > 0))
	{
		dump("%d: %d", max_index, max_value);
		PerformMove(max_index, max_value, hWnd, pt, target);
	}
}

static void DoMinDeltaMove(HWND hWnd, LPPOINT pt, LPRECT target, LPRECT parent)
{
	move moves[NUM_DIR];
	int min_value = INT_MAX;
	direction min_index = NUM_DIR;
	direction i;

	dump("MinDelta");
	for (i = LEFT; i < NUM_DIR; i++)
		CalculateMove(i, moves + i, pt, target, parent);

	for (i = LEFT; i < NUM_DIR; i++)
	{
		if (moves[i].valid && (moves[i].distance < min_value))
		{
			min_value = moves[i].distance;
			min_index = i;
		}
	}

	if (min_index < NUM_DIR)
	{
		dump("%d: %d, %d", min_index, min_value, moves[min_index].margin);
		PerformMove(min_index, moves[min_index].margin, hWnd, pt, target);
	}
}

DllExport LRESULT CALLBACK MoverProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0)
	{
		LPMOUSEHOOKSTRUCT pMHS = (LPMOUSEHOOKSTRUCT) lParam;
		char szClass[32];
		LONG style = GetWindowLong(pMHS->hwnd, GWL_STYLE);
		if ((style & WS_CHILD) &&									// child window
			GetClassName(pMHS->hwnd, szClass, sizeof(szClass)) &&	// can get class name
			!lstrcmpi("BUTTON", szClass) &&							// it is a button
			!(style & 0x0000FFFEL))									// push button or dafult push buton
		{
			RECT target, parent;
			POINT pt;
			static BOOL bIsMaxMargin = TRUE;

			GetAdjustedWindowRect(pMHS->hwnd, &target);
			GetClientRect(GetParent(pMHS->hwnd), &parent);
			pt = pMHS->pt;
			ScreenToClient(GetParent(pMHS->hwnd), &pt);

			dump("Point (%d, %d)", pt.x, pt.y);
			dump("Window 0x%X (%d, %d) - (%d, %d)", pMHS->hwnd, target.left, target.top, target.right, target.bottom);
			dump("Parent 0x%X (%d, %d) - (%d, %d)", GetParent(pMHS->hwnd), parent.left, parent.top, parent.right, parent.bottom);

			if (bIsMaxMargin)
				DoMaxMarginMove(pMHS->hwnd, &pt, &target, &parent);
			else
				DoMinDeltaMove(pMHS->hwnd, &pt, &target, &parent);

			bIsMaxMargin = !bIsMaxMargin;
			return TRUE; // the button never gets the message
		}
	}

	return CallNextHookEx(hHook, nCode, wParam, lParam);
}

DllExport HHOOK CALLBACK RegisterMover(void)
{
	return (hHook = SetWindowsHookEx(WH_MOUSE, MoverProc, hDLL, 0));
}

DllExport void CALLBACK UnregisterMover(void)
{
	UnhookWindowsHookEx(hHook);
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reason, LPVOID lpReserved)
{
	switch(reason)
	{
	case DLL_PROCESS_ATTACH:
		hDLL = hModule;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}
