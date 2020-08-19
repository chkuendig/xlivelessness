#include "windows.h"
#include "xlln.hpp"
#include "debug-text.hpp"
#include "../xlive/xdefs.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/xlocator.hpp"
#include "../xlive/xrender.hpp"
#include "../xlive/xsocket.hpp"
#include "rand-name.hpp"
#include "../resource.h"
#include <string>
#include <time.h>

static LRESULT CALLBACK DLLWindowProc(HWND, UINT, WPARAM, LPARAM);

static HINSTANCE xlln_hModule = NULL;
HWND xlln_window_hwnd = NULL;
static HMENU xlln_window_hMenu = NULL;
static int xlln_instance = 0;

static DWORD xlln_login_player = 0;
static DWORD xlln_login_player_h[] = { MYMENU_LOGIN1, MYMENU_LOGIN2, MYMENU_LOGIN3, MYMENU_LOGIN4 };

BOOL xlln_debug = FALSE;

static CRITICAL_SECTION xlive_critsec_recvfrom_handler_funcs;
static std::map<DWORD, char*> xlive_recvfrom_handler_funcs;

INT WINAPI XSocketRecvFromCustomHelper(INT result, SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen)
{
	EnterCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
	typedef INT(WINAPI *tXliveRecvfromHandler)(INT result, SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen);
	for (std::map<DWORD, char*>::iterator it = xlive_recvfrom_handler_funcs.begin(); it != xlive_recvfrom_handler_funcs.end(); it++) {
		if (result <= 0) {
			break;
		}
		tXliveRecvfromHandler handlerFunc = (tXliveRecvfromHandler)it->first;
		result = handlerFunc(result, s, buf, len, flags, from, fromlen);
	}
	LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
	if (result != 0) {
		// TODO
		addDebugText("result not 0!");
	}
	return 0;
}


static HMENU CreateDLLWindowMenu(HINSTANCE hModule)
{
	HMENU hMenu;
	hMenu = CreateMenu();
	HMENU hMenuPopup;
	if (hMenu == NULL)
		return FALSE;

	hMenuPopup = CreatePopupMenu();
	AppendMenu(hMenuPopup, MF_STRING, MYMENU_EXIT, TEXT("Exit"));
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, TEXT("File"));

	hMenuPopup = CreatePopupMenu();
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_LOGIN1, TEXT("Login P1"));
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_LOGIN2, TEXT("Login P2"));
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_LOGIN3, TEXT("Login P3"));
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_LOGIN4, TEXT("Login P4"));
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, TEXT("Login"));

	hMenuPopup = CreatePopupMenu();
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_ALWAYSTOP, TEXT("Always on top"));
	AppendMenu(hMenuPopup, MF_STRING, MYMENU_ABOUT, TEXT("About"));
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, TEXT("Help"));

	CheckMenuItem(hMenu, xlln_login_player_h[0], MF_CHECKED);
	//EnableMenuItem(hMenu, MYMENU_LOGIN2, MF_GRAYED);
	//EnableMenuItem(hMenu, MYMENU_LOGIN3, MF_GRAYED);
	//EnableMenuItem(hMenu, MYMENU_LOGIN4, MF_GRAYED);


	return hMenu;
}

static DWORD WINAPI ThreadProc(LPVOID lpParam)
{
	srand((unsigned int)time(NULL));

	const wchar_t* windowclassname = L"XLLNDLLWindowClass";
	HINSTANCE hModule = reinterpret_cast<HINSTANCE>(lpParam);
	xlln_window_hMenu = CreateDLLWindowMenu(hModule);

	// Register the windows Class.
	WNDCLASSEXW wc;
	wc.hInstance = hModule;
	wc.lpszClassName = windowclassname;
	wc.lpfnWndProc = DLLWindowProc;
	wc.style = CS_DBLCLKS;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszMenuName = NULL;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)COLOR_BACKGROUND;
	if (!RegisterClassExW(&wc))
		return FALSE;

	wchar_t title[80];
	swprintf_s(title, 80, L"XLLN v%d.%d.%d.%d", DLL_VERSION);

	HWND hwdParent = NULL;// FindWindowW(L"Window Injected Into ClassName", L"Window Injected Into Caption");
	xlln_window_hwnd = CreateWindowExW(0, windowclassname, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, xlln_debug ? 700 : 165, hwdParent, xlln_window_hMenu, hModule, NULL);
	ShowWindow(xlln_window_hwnd, xlln_debug ? SW_NORMAL : SW_HIDE);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		// Handle tab ordering
		if (!IsDialogMessage(xlln_window_hwnd, &msg)) {
			// Translate virtual-key msg into character msg
			TranslateMessage(&msg);
			// Send msg to WindowProcedure(s)
			DispatchMessage(&msg);
		}
	}
	return TRUE;
}


// #41140
DWORD WINAPI XLLNLogin(DWORD dwUserIndex, BOOL bLiveEnabled, DWORD dwUserId, const CHAR *szUsername)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (szUsername && (!*szUsername || strlen(szUsername) > XUSER_MAX_NAME_LENGTH))
		return ERROR_INVALID_ACCOUNT_NAME;
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_NotSignedIn)
		return ERROR_ALREADY_ASSIGNED;

	if (!dwUserId) {
		//Generate Random Number?
		dwUserId = rand();
	}
	if (szUsername) {
		strncpy_s(xlive_users_info[dwUserIndex]->szUserName, XUSER_NAME_SIZE, szUsername, XUSER_NAME_SIZE);
	}
	else {
		wchar_t generated_name[XUSER_NAME_SIZE];
		GetName(generated_name, XUSER_NAME_SIZE);
		snprintf(xlive_users_info[dwUserIndex]->szUserName, XUSER_NAME_SIZE, "%ls", generated_name);
	}

	xlive_users_info[dwUserIndex]->UserSigninState = bLiveEnabled ? eXUserSigninState_SignedInToLive : eXUserSigninState_SignedInLocally;
	//0x0009000000000000 - online xuid?
	xlive_users_info[dwUserIndex]->xuid = 0xE000007300000000 + dwUserId;
	xlive_users_info[dwUserIndex]->dwInfoFlags;

	xlive_users_info_changed[dwUserIndex] = TRUE;

	SetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_USERNAME, xlive_users_info[dwUserIndex]->szUserName);
	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE, bLiveEnabled ? BST_CHECKED : BST_UNCHECKED);

	BOOL checked = TRUE;//GetMenuState(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], 0) != MF_CHECKED;
	//CheckMenuItem(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], checked ? MF_CHECKED : MF_UNCHECKED);

	ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGIN), checked ? SW_HIDE : SW_SHOWNORMAL);
	ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGOUT), checked ? SW_SHOWNORMAL : SW_HIDE);

	return ERROR_SUCCESS;
}

// #41141
DWORD WINAPI XLLNLogout(DWORD dwUserIndex)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn)
		return ERROR_NOT_LOGGED_ON;

	xlive_users_info[dwUserIndex]->UserSigninState = eXUserSigninState_NotSignedIn;
	xlive_users_info_changed[dwUserIndex] = TRUE;

	BOOL checked = FALSE;//GetMenuState(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], 0) != MF_CHECKED;
	//CheckMenuItem(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], checked ? MF_CHECKED : MF_UNCHECKED);

	ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGIN), checked ? SW_HIDE : SW_SHOWNORMAL);
	ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGOUT), checked ? SW_SHOWNORMAL : SW_HIDE);

	return ERROR_SUCCESS;
}

// #41142
DWORD WINAPI XLLNModifyProperty(XLLNModifyPropertyTypes::TYPE propertyId, DWORD *newValue, DWORD *oldValue)
{
	TRACE_FX();
	if (propertyId == XLLNModifyPropertyTypes::tUNKNOWN)
		return ERROR_INVALID_PARAMETER;

	if (propertyId == XLLNModifyPropertyTypes::tFPS_LIMIT) {
		if (oldValue && !newValue) {
			*oldValue = xlive_fps_limit;
		}
		else if (newValue) {
			DWORD old_value = SetFPSLimit(*newValue);
			if (oldValue) {
				*oldValue = old_value;
			}
		}
		else {
			return ERROR_NOT_SUPPORTED;
		}
		return ERROR_SUCCESS;
	}
	else if (propertyId == XLLNModifyPropertyTypes::tCUSTOM_LOCAL_USER_hIPv4) {
		EnterCriticalSection(&xlive_critsec_custom_local_user_hipv4);
		if (newValue && *newValue == INADDR_NONE) {
			LeaveCriticalSection(&xlive_critsec_custom_local_user_hipv4);
			return ERROR_INVALID_PARAMETER;
		}
		if (!newValue && !oldValue) {
			if (xlive_custom_local_user_hipv4 == INADDR_NONE) {
				LeaveCriticalSection(&xlive_critsec_custom_local_user_hipv4);
				return ERROR_NOT_FOUND;
			}
			xlive_custom_local_user_hipv4 = INADDR_NONE;
			LeaveCriticalSection(&xlive_critsec_custom_local_user_hipv4);
			return ERROR_SUCCESS;
		}
		if (newValue && !oldValue && xlive_custom_local_user_hipv4 != INADDR_NONE) {
			LeaveCriticalSection(&xlive_critsec_custom_local_user_hipv4);
			return ERROR_ALREADY_REGISTERED;
		}
		if (oldValue) {
			*oldValue = xlive_custom_local_user_hipv4;
		}
		if (newValue) {
			xlive_custom_local_user_hipv4 = *newValue;
		}
		LeaveCriticalSection(&xlive_critsec_custom_local_user_hipv4);
		return ERROR_SUCCESS;
	}
	else if (propertyId == XLLNModifyPropertyTypes::tLiveOverLan_BROADCAST_HANDLER) {
		EnterCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
		if (!newValue && !oldValue) {
			if (liveoverlan_broadcast_handler == NULL) {
				LeaveCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
				return ERROR_NOT_FOUND;
			}
			liveoverlan_broadcast_handler = NULL;
			LeaveCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
			return ERROR_SUCCESS;
		}
		if (newValue && !oldValue && liveoverlan_broadcast_handler != NULL) {
			LeaveCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
			return ERROR_ALREADY_REGISTERED;
		}
		if (oldValue) {
			*oldValue = (DWORD)liveoverlan_broadcast_handler;
		}
		if (newValue) {
			liveoverlan_broadcast_handler = (VOID(WINAPI*)(LIVE_SERVER_DETAILS*))*newValue;
		}
		LeaveCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
		return ERROR_SUCCESS;
	}
	else if (propertyId == XLLNModifyPropertyTypes::tRECVFROM_CUSTOM_HANDLER_REGISTER) {
		// TODO
		XLLNModifyPropertyTypes::RECVFROM_CUSTOM_HANDLER_REGISTER *handler = (XLLNModifyPropertyTypes::RECVFROM_CUSTOM_HANDLER_REGISTER*)newValue;
		addDebugText(handler->Identifier);
		DWORD idLen = strlen(handler->Identifier) + 1;
		char *identifier = (char*)malloc(sizeof(char) * idLen);
		strcpy_s(identifier, idLen, handler->Identifier);

		if (oldValue && newValue) {
			return ERROR_INVALID_PARAMETER;
		}
		EnterCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
		if (oldValue) {
			if (!xlive_recvfrom_handler_funcs.count((DWORD)oldValue)) {
				LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
				return ERROR_NOT_FOUND;
			}
			xlive_recvfrom_handler_funcs.erase((DWORD)oldValue);
		}
		else {
			if (xlive_recvfrom_handler_funcs.count((DWORD)handler->FuncPtr)) {
				LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
				return ERROR_ALREADY_REGISTERED;
			}
			xlive_recvfrom_handler_funcs[(DWORD)handler->FuncPtr] = identifier;
		}
		LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
		return ERROR_SUCCESS;
	}
	else if (propertyId == XLLNModifyPropertyTypes::tRECVFROM_CUSTOM_HANDLER_UNREGISTER) {
		// TODO
	}

	return ERROR_UNKNOWN_PROPERTY;
}

// #41143
DWORD WINAPI XLLNDebugLog(DWORD logLevel, const char *message)
{
	addDebugText(message);
	return ERROR_SUCCESS;
}
/*
std::string formattestthing(const char *const format, ...)
{
	auto temp = std::vector<char>{};
	auto length = std::size_t{ 2 };
	va_list args;
	while (temp.size() <= length)
	{
		temp.resize(length + 1);
		va_start(args, format);
		const auto status = std::vsnprintf(temp.data(), temp.size(), format, args);
		va_end(args);
		if (status < 0)
			throw std::runtime_error{ "string formatting error" };
		length = static_cast<std::size_t>(status);
	}
	return std::string{ temp.data(), length };
}

// #41144
DWORD WINAPI XLLNDebugLogF(DWORD logLevel, const char *const format, ...)
{
	va_list args;
	va_start(args, format);
	va_list args2;
	va_copy(args2, args);
	std::string message = formattestthing(format, args2);
	va_end(args);
	addDebugText(message.c_str());
	return ERROR_SUCCESS;
}*/

// #41144
DWORD WINAPI XLLNDebugLogF(DWORD logLevel, const char *const format, ...)
{
	auto temp = std::vector<char>{};
	auto length = std::size_t{ 63 };
	va_list args;
	while (temp.size() <= length) {
		temp.resize(length + 1);
		va_start(args, format);
		const auto status = std::vsnprintf(temp.data(), temp.size(), format, args);
		va_end(args);
		if (status < 0) {
			// string formatting error.
			return ERROR_INVALID_PARAMETER;
		}
		length = static_cast<std::size_t>(status);
	}
	addDebugText(std::string{ temp.data(), length }.c_str());
	return ERROR_SUCCESS;
}

static void UpdateUserInputBoxes(DWORD dwUserIndex)
{
	for (DWORD i = 0; i < 4; i++) {
		if (i == dwUserIndex) {
			continue;
		}
		BOOL checked = FALSE;//GetMenuState(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], 0) != MF_CHECKED;
		CheckMenuItem(xlln_window_hMenu, xlln_login_player_h[i], checked ? MF_CHECKED : MF_UNCHECKED);
	}
	BOOL checked = TRUE;
	CheckMenuItem(xlln_window_hMenu, xlln_login_player_h[dwUserIndex], checked ? MF_CHECKED : MF_UNCHECKED);

	checked = FALSE;
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_NotSignedIn)
		checked = TRUE;
	BOOL bLiveEnabled = FALSE;
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_SignedInToLive)
		bLiveEnabled = TRUE;

	SetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_USERNAME, xlive_users_info[dwUserIndex]->szUserName);
	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE, bLiveEnabled ? BST_CHECKED : BST_UNCHECKED);

	ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGIN), checked ? SW_HIDE : SW_SHOWNORMAL);
	ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGOUT), checked ? SW_SHOWNORMAL : SW_HIDE);
}

static LRESULT CALLBACK DLLWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_PAINT) {
		/*PAINTSTRUCT ps;
		HDC hdc = BeginPaint(xlln_window_hwnd, &ps);
		TCHAR greeting[] = TEXT("Hello, World!");
		TextOut(hdc, 5, 5, greeting, strlen(greeting));

		EndPaint(xlln_window_hwnd, &ps);*/
	}
	else if (message == WM_SYSCOMMAND) {
		if (wParam == SC_CLOSE) {
			ShowXLLN(XLLN_SHOW_HIDE);
			return 0;
		}
	}
	else if (message == WM_COMMAND) {
		if (wParam == MYMENU_EXIT) {
			//SendMessage(hwnd, WM_CLOSE, 0, 0);
			LiveOverLanAbort();
			exit(EXIT_SUCCESS);
		}
		else if (wParam == MYMENU_ALWAYSTOP) {
			BOOL checked = GetMenuState(xlln_window_hMenu, MYMENU_ALWAYSTOP, 0) != MF_CHECKED;
			if (checked) {
				SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			}
			else {
				SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			}
			CheckMenuItem(xlln_window_hMenu, MYMENU_ALWAYSTOP, checked ? MF_CHECKED : MF_UNCHECKED);
		}
		else if (wParam == MYMENU_ABOUT) {
			MessageBoxA(hwnd,
"Created by Glitchy Scripts,\n\
with thanks to PermaNulled.\n\
\n\
Executable Launch Parameters:\n\
-xlivefps=<uint> ? 0 to disable fps limiter (default 60).\n\
-xllndebug ? Sleep until debugger attach.\n\
-xllndebuglog ? Enable debug log.\n\
-xlivedebug ? Sleep XLiveInitialize until debugger attach.\n\
-xlivenetdisable ? Disable all network functionality.\n\
-xliveportbase=<ushort> ? Change the Base Port (default 2000).\n\
-xllndebuglogblacklist=<function_name,func_name,...> ? Exclude the listed function names from the debug log."
				, "About", MB_OK);
		}
		else if (wParam == MYMENU_LOGIN1) {
			xlln_login_player = 0;
			UpdateUserInputBoxes(xlln_login_player);
		}
		else if (wParam == MYMENU_LOGIN2) {
			xlln_login_player = 1;
			UpdateUserInputBoxes(xlln_login_player);
		}
		else if (wParam == MYMENU_LOGIN3) {
			xlln_login_player = 2;
			UpdateUserInputBoxes(xlln_login_player);
		}
		else if (wParam == MYMENU_LOGIN4) {
			xlln_login_player = 3;
			UpdateUserInputBoxes(xlln_login_player);
		}
		else if (wParam == MYWINDOW_CHK_LIVEENABLE) {
			BOOL checked = IsDlgButtonChecked(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE) != BST_CHECKED;
			CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE, checked ? BST_CHECKED : BST_UNCHECKED);
		}
		else if (wParam == MYWINDOW_BTN_LOGIN) {
			char jlbuffer[16];
			GetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_USERNAME, jlbuffer, 16);
			SetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_USERNAME, jlbuffer);

			BOOL live_enabled = IsDlgButtonChecked(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE) == BST_CHECKED;
			DWORD result_login = XLLNLogin(xlln_login_player, live_enabled, NULL, strnlen_s(jlbuffer, 16) == 0 ? NULL : jlbuffer);
		}
		else if (wParam == MYWINDOW_BTN_LOGOUT) {
			DWORD result_logout = XLLNLogout(xlln_login_player);
		}
		else if (wParam == MYWINDOW_BTN_TEST) {
			// WIP.
			extern bool xlive_invite_to_game;
			xlive_invite_to_game = true;
		}
		return 0;
	}
	else if (message == WM_CTLCOLORSTATIC) {
		HDC hdc = reinterpret_cast<HDC>(wParam);
		SetTextColor(hdc, RGB(0, 0, 0));
		SetBkColor(hdc, 0x00C8C8C8);
		return (INT_PTR)CreateSolidBrush(0x00C8C8C8);
	}
	else if (message == WM_CREATE) {

		HWND hWndControl;

		CreateWindowA("edit", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP,
			10, 10, 260, 22, hwnd, (HMENU)MYWINDOW_TBX_USERNAME, xlln_hModule, NULL);

		hWndControl = CreateWindowA("button", "Live Enabled", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			10, 40, 150, 22, hwnd, (HMENU)MYWINDOW_CHK_LIVEENABLE, xlln_hModule, NULL);

		CreateWindowA("button", "Login", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			10, 70, 75, 25, hwnd, (HMENU)MYWINDOW_BTN_LOGIN, xlln_hModule, NULL);

		CreateWindowA("button", "Logout", WS_CHILD | WS_TABSTOP,
			10, 70, 75, 25, hwnd, (HMENU)MYWINDOW_BTN_LOGOUT, xlln_hModule, NULL);

		hWndControl = CreateWindowA("edit", "", WS_CHILD | (xlln_debug ? WS_VISIBLE : 0) | WS_BORDER | ES_MULTILINE | WS_SIZEBOX | WS_TABSTOP | WS_HSCROLL,
			10, 120, 350, 500, hwnd, (HMENU)MYWINDOW_TBX_TEST, xlln_hModule, NULL);

		if (xlln_debug) {
			CreateWindowA("button", "Test", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				85, 70, 75, 25, hwnd, (HMENU)MYWINDOW_BTN_TEST, xlln_hModule, NULL);
		}

	}
	else if (message == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	else if (message == WM_CLOSE) {
		// Stupid textbox causes the window to close.
		return 0;
	}
	
	return DefWindowProcW(hwnd, message, wParam, lParam);
}

INT ShowXLLN(DWORD dwShowType)
{
	if (dwShowType == XLLN_SHOW_HIDE) {
		ShowWindow(xlln_window_hwnd, SW_HIDE);
	}
	else if (dwShowType == XLLN_SHOW_HOME) {
		ShowWindow(xlln_window_hwnd, SW_SHOWNORMAL);
		SetWindowPos(xlln_window_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	else if (dwShowType == XLLN_SHOW_LOGIN) {
		ShowWindow(xlln_window_hwnd, SW_SHOWNORMAL);
		SetWindowPos(xlln_window_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		SendMessage(xlln_window_hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(xlln_window_hwnd, MYWINDOW_TBX_USERNAME), TRUE);
	}
	return ERROR_SUCCESS;
}

INT InitXLLN(HMODULE hModule)
{
	BOOL xlln_debug_pause = FALSE;

	int nArgs;
	LPWSTR* lpwszArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	if (lpwszArglist != NULL) {
		for (int i = 1; i < nArgs; i++) {
			if (wcscmp(lpwszArglist[i], L"-xllndebug") == 0) {
				xlln_debug_pause = TRUE;
			}
			else if (wcscmp(lpwszArglist[i], L"-xllndebuglog") == 0) {
				xlln_debug = TRUE;
			}
		}
	}

	while (xlln_debug_pause && !IsDebuggerPresent()) {
		Sleep(500L);
	}

	InitializeCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
	InitializeCriticalSection(&xlive_critsec_custom_local_user_hipv4);
	InitializeCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);

	wchar_t mutex_name[40];
	DWORD mutex_last_error;
	HANDLE mutex = NULL;
	do {
		if (mutex) {
			mutex_last_error = CloseHandle(mutex);
		}
		xlln_instance++;
		swprintf(mutex_name, 40, L"Global\\XLLNInstance#%d", xlln_instance);
		mutex = CreateMutexW(0, FALSE, mutex_name);
		mutex_last_error = GetLastError();
	} while (mutex_last_error != ERROR_SUCCESS);

	INT error_DebugLog = InitDebugLog(xlln_instance);

	if (lpwszArglist != NULL) {
		for (int i = 1; i < nArgs; i++) {
			if (wcsstr(lpwszArglist[i], L"-xlivefps=") != NULL) {
				DWORD tempuint = 0;
				if (swscanf_s(lpwszArglist[i], L"-xlivefps=%u", &tempuint) == 1) {
					SetFPSLimit(tempuint);
				}
			}
			else if (wcscmp(lpwszArglist[i], L"-xlivedebug") == 0) {
				xlive_debug_pause = TRUE;
			}
			else if (wcscmp(lpwszArglist[i], L"-xlivenetdisable") == 0) {
				xlive_netsocket_abort = TRUE;
			}
			else if (wcsstr(lpwszArglist[i], L"-xliveportbase=") != NULL) {
				WORD tempuint = 0;
				if (swscanf_s(lpwszArglist[i], L"-xliveportbase=%hu", &tempuint) == 1) {
					xlive_base_port = tempuint;
				}
			}
			else if (wcsstr(lpwszArglist[i], L"-xllndebuglogblacklist=") != NULL) {
				wchar_t *blacklist = &lpwszArglist[i][23];
				while (true) {
					wchar_t *next_item = wcschr(blacklist, L',');
					if (next_item) {
						next_item++[0] = 0;
					}
					if (blacklist[0] != 0 && blacklist[0] != L',') {
						int black_text_len = wcsnlen_s(blacklist, 0x1000 - 1) + 1;
						char *black_text = (char*)malloc(sizeof(char) * black_text_len);
						snprintf(black_text, black_text_len, "%ws", blacklist);
						if (!addDebugTextBlacklist(black_text)) {
							free(black_text);
						}
					}
					if (!next_item || !next_item[0]) {
						break;
					}
					blacklist = next_item;
				}
			}
		}
	}
	LocalFree(lpwszArglist);

	xlln_hModule = hModule;
	CreateThread(0, NULL, ThreadProc, (LPVOID)hModule, NULL, NULL);

	xlive_title_id = 0;
	addDebugText("ERROR: TODO title.cfg not found. Default Title ID set.");

	return 0;
}

INT UninitXLLN()
{
	INT error_DebugLog = UninitDebugLog();

	DeleteCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
	DeleteCriticalSection(&xlive_critsec_custom_local_user_hipv4);
	DeleteCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
	return 0;
}
